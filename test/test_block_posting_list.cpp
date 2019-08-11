#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "test_generic_sequence.hpp"

#include "codec/block_codecs.hpp"
#include "codec/maskedvbyte.hpp"
#include "codec/streamvbyte.hpp"
#include "codec/qmx.hpp"
#include "codec/varintgb.hpp"
#include "codec/simple8b.hpp"
#include "codec/simple16.hpp"
#include "codec/simdbp.hpp"

#include "block_posting_list.hpp"

#include <vector>
#include <cstdlib>
#include <algorithm>
#include <random>


template <typename Cursor>
void test_block_posting_list_ops(uint8_t const* data, uint64_t n, uint64_t universe,
                                 std::vector<uint64_t> const& docs,
                                 std::vector<uint64_t> const& freqs)
{
    auto e = Cursor::from(data, universe);
    REQUIRE(n == e.size());
    for (size_t i = 0; i < n; ++i, e.next()) {
        MY_REQUIRE_EQUAL(docs[i], e.docid(), "i = " << i << " size = " << n);
        MY_REQUIRE_EQUAL(freqs[i], e.freq(), "i = " << i << " size = " << n);
    }
    // XXX better testing of next_geq
    for (size_t i = 0; i < n; ++i) {
        e.reset();
        e.next_geq(docs[i]);
        MY_REQUIRE_EQUAL(docs[i], e.docid(), "i = " << i << " size = " << n);
        MY_REQUIRE_EQUAL(freqs[i], e.freq(), "i = " << i << " size = " << n);
    }
    e.reset();
    e.next_geq(docs.back() + 1);
    REQUIRE(universe == e.docid());
    e.reset();
    e.next_geq(universe);
    REQUIRE(universe == e.docid());
}

void random_posting_data(uint64_t n, uint64_t universe,
                         std::vector<uint64_t>& docs,
                         std::vector<uint64_t>& freqs)
{
    docs = random_sequence(universe, n, true);
    freqs.resize(n);
    std::generate(freqs.begin(), freqs.end(),
                  []() { return (rand() % 256) + 1; });
}

template <typename BlockCodec>
void test_block_posting_list()
{
    uint64_t universe = 20000;
    for (size_t t = 0; t < 20; ++t) {
        double avg_gap = 1.1 + double(rand()) / RAND_MAX * 10;
        uint64_t n = uint64_t(universe / avg_gap);

        std::vector<uint64_t> docs, freqs;
        random_posting_data(n, universe, docs, freqs);
        std::vector<uint8_t> data;
        pisa::write_block_posting_list<BlockCodec>(data, n, docs.begin(), freqs.begin());

        test_block_posting_list_ops<pisa::BlockPostingCursor<BlockCodec>>(
            data.data(), n, universe, docs, freqs);
    }
}

template <typename BlockCodec>
void test_block_posting_list_reordering()
{
    uint64_t universe = 20000;
    for (size_t t = 0; t < 20; ++t) {
        double avg_gap = 1.1 + double(rand()) / RAND_MAX * 10;
        uint64_t n = uint64_t(universe / avg_gap);

        std::vector<uint64_t> docs, freqs;
        random_posting_data(n, universe, docs, freqs);
        std::vector<uint8_t> data;
        pisa::write_block_posting_list<BlockCodec>(data, n, docs.begin(), freqs.begin());

        // reorder blocks
        auto e = pisa::BlockPostingCursor<BlockCodec>::from(data.data(), universe);
        auto blocks = e.get_blocks();
        std::shuffle(blocks.begin() + 1,
                     blocks.end(),
                     std::mt19937(std::random_device()())); // leave first block in place

        std::vector<uint8_t> reordered_data;
        pisa::write_blocks(reordered_data, n, blocks);

        test_block_posting_list_ops<pisa::BlockPostingCursor<BlockCodec>>(
            reordered_data.data(), n, universe, docs, freqs);
    }
}

TEST_CASE("block_posting_list")
{
    test_block_posting_list<pisa::optpfor_block>();
    test_block_posting_list<pisa::varint_G8IU_block>();
    test_block_posting_list<pisa::streamvbyte_block>();
    test_block_posting_list<pisa::maskedvbyte_block>();
    test_block_posting_list<pisa::varintgb_block>();
    test_block_posting_list<pisa::interpolative_block>();
    test_block_posting_list<pisa::qmx_block>();
    test_block_posting_list<pisa::simple8b_block>();
    test_block_posting_list<pisa::simple16_block>();
    test_block_posting_list<pisa::simdbp_block>();
}
TEST_CASE("block_posting_list_reordering")
{
    test_block_posting_list_reordering<pisa::optpfor_block>();
}
