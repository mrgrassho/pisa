#pragma once

#include <random>
#include <string>
#include <unordered_map>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <gsl/span>
#include <pstl/algorithm>
#include <pstl/execution>
#include <pstl/numeric>
#include <range/v3/action/shuffle.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>
#include <tbb/task_scheduler_init.h>

#include "invert.hpp"
#include "io.hpp"
#include "type_safe.hpp"
#include "vector.hpp"

namespace pisa {

using pisa::literals::operator""_d;
using pisa::literals::operator""_s;

auto mapping_from_files(std::istream *full_titles, gsl::span<std::istream *> shard_titles)
    -> Vector<Document_Id, Shard_Id>
{
    std::unordered_map<std::string, Shard_Id> map;
    Shard_Id shard_id{};
    for (auto *is : shard_titles) {
        io::for_each_line(*is, [&](auto const &title) {
            if (auto pos = map.find(title); pos == map.end()) {
                map[title] = shard_id;
            }
            else {
                spdlog::warn(
                    "Document {} already belongs to shard {}: mapping for shard {} ignored",
                    title,
                    pos->second.as_int(),
                    shard_id);
            }
        });
        shard_id += 1;
    }

    Vector<Document_Id, Shard_Id> result;
    result.reserve(map.size());
    io::for_each_line(*full_titles, [&](auto const &title) {
        if (auto pos = map.find(title); pos != map.end()) {
            result.push_back(pos->second);
        }
        else {
            spdlog::warn("No shard assignment for document {}; will be assigned to shard 0", title);
            result.push_back(0_s);
        }
    });
    return result;
}

auto mapping_from_files(std::string const &full_titles, gsl::span<std::string const> shard_titles)
    -> Vector<Document_Id, Shard_Id>
{
    std::ifstream fis(full_titles);
    std::vector<std::unique_ptr<std::ifstream>> shard_is;
    std::vector<std::istream *> shard_pointers;
    for (auto const &shard_file : shard_titles) {
        shard_is.push_back(std::make_unique<std::ifstream>(shard_file));
        shard_pointers.push_back(shard_is.back().get());
    }
    return mapping_from_files(&fis, gsl::span<std::istream *>(shard_pointers));
}

auto create_random_mapping(int document_count,
                           int shard_count,
                           std::optional<std::uint64_t> seed = std::nullopt)
    -> Vector<Document_Id, Shard_Id>
{
    std::random_device rd;
    std::mt19937 g(seed.value_or(rd()));
    Vector<Document_Id, Shard_Id> mapping(document_count);
    auto shard_size = ceil_div(document_count, shard_count);
    auto documents = ranges::view::iota(0_d, Document_Id{document_count}) | ranges::to_vector
                     | ranges::action::shuffle(g);

    ranges::for_each(ranges::view::zip(ranges::view::iota(Shard_Id{0u}, Shard_Id{shard_count}),
                                       ranges::view::chunk(documents, shard_size)),
                     [&](auto &&entry) {
                         auto &&[shard_id, shard_documents] = entry;
                         for (auto document : shard_documents) {
                             mapping[document] = shard_id;
                         }
                     });
    return mapping;
}

auto create_random_mapping(std::string const &input_basename,
                           int shard_count,
                           std::optional<std::uint64_t> seed = std::nullopt)
    -> Vector<Document_Id, Shard_Id>
{
    auto document_count = *(*binary_collection(input_basename.c_str()).begin()).begin();
    return create_random_mapping(document_count, shard_count, seed);
}

auto build_shard(std::string const &input_basename,
                 std::string const &output_basename,
                 Shard_Id shard_id,
                 std::vector<Document_Id> documents,
                 Vector<Term_Id, std::string> const &terms)
{
    auto fwd = binary_collection(input_basename.c_str());
    documents |= ranges::action::sort;

    std::ofstream os(fmt::format("{}.{:03d}", output_basename, shard_id.as_int()));

    auto document_count = static_cast<uint32_t>(documents.size());
    write_sequence(os, gsl::make_span<uint32_t const>(&document_count, 1));

    auto for_each_document_in_shard = [&](auto fn) {
        auto docid_iter = documents.begin();
        Document_Id current_document{0};

        for (auto fwd_iter = ++fwd.begin(); fwd_iter != fwd.end();
             std::advance(fwd_iter, 1), current_document += 1)
        {
            if (current_document != *docid_iter) {
                continue;
            }

            auto seq = *fwd_iter;
            fn(seq);

            std::advance(docid_iter, 1);
            if (docid_iter == documents.end()) {
                break;
            }
        }
    };

    std::vector<uint32_t> has_term(terms.size(), 0u);
    for_each_document_in_shard([&](auto const &sequence) {
        for (auto term : sequence) {
            has_term[term] = 1u;
        }
    });

    std::ofstream tos(fmt::format("{}.{:03d}.terms", output_basename, shard_id.as_int()));
    for (auto const &[term, occurs] : ranges::view::zip(terms, has_term)) {
        if (occurs) {
            tos << term << '\n';
        }
    }

    std::exclusive_scan(
        std::execution::seq, has_term.begin(), has_term.end(), has_term.begin(), 0u);
    auto remapped_term_id = [&](auto term) { return has_term[term]; };
    for_each_document_in_shard([&](auto const &sequence) {
        std::vector<uint32_t> terms(sequence.size());
        std::transform(sequence.begin(), sequence.end(), terms.begin(), remapped_term_id);
        write_sequence(os, gsl::span<uint32_t const>(&terms[0], terms.size()));
    });

    std::ifstream dis(fmt::format("{}.documents", input_basename));
    std::ofstream dos(fmt::format("{}.{:03d}.documents", output_basename, shard_id.as_int()));
    auto docid_iter = documents.begin();
    Document_Id current_document{0};
    pisa::io::for_each_line(dis, [&](std::string const & document_title) {
        if (docid_iter == documents.end() || current_document != *docid_iter) {
            current_document += 1;
            return;
        }
        dos << document_title << '\n';
        if (docid_iter != documents.end()) {
            std::advance(docid_iter, 1);
            current_document += 1;
        }
    });
}

auto partition_fwd_index(std::string const &input_basename,
                         std::string const &output_basename,
                         Vector<Document_Id, Shard_Id> &mapping)
{
    spdlog::info("Partitioning titles");
    auto terms = io::read_type_safe_string_vector<Term_Id>(fmt::format("{}.terms", input_basename));

    spdlog::info("Partitioning documents");
    std::vector<std::pair<Shard_Id, std::vector<Document_Id>>> shard_documents;
    {
        auto mapped_pairs = mapping.entries() | ranges::to_vector |
                            ranges::action::sort([](auto const &lhs, auto const &rhs) {
                                return lhs.second < rhs.second;
                            });
        auto pos = mapped_pairs.begin();
        while (pos != mapped_pairs.end()) {
            shard_documents.emplace_back(pos->second, std::vector<Document_Id>{});
            auto next = std::find_if(
                pos, mapped_pairs.end(), [&, shard_id = pos->second](auto const &entry) {
                    if (entry.second != shard_id) {
                        return true;
                    }
                    shard_documents.back().second.push_back(entry.first);
                    return false;
                });
            pos = next;
        };
    }

    std::for_each(
        std::execution::par_unseq,
        shard_documents.begin(),
        shard_documents.end(),
        [&](auto &&entry) {
            build_shard(
                input_basename, output_basename, entry.first, std::move(entry.second), terms);
        });

    spdlog::info("Partitioning terms");
}

} // namespace pisa
