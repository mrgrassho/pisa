#pragma once

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#include "bit_vector.hpp"
#include "block_posting_list.hpp"
#include "codec/block_codec.hpp"
#include "codec/block_codecs.hpp"
#include "codec/compact_elias_fano.hpp"
#include "codec/maskedvbyte.hpp"
#include "codec/qmx.hpp"
#include "codec/simdbp.hpp"
#include "codec/simple16.hpp"
#include "codec/simple8b.hpp"
#include "codec/streamvbyte.hpp"
#include "codec/varintgb.hpp"
#include "mappable/mappable_vector.hpp"
#include "mixed_block.hpp"

namespace pisa {

template <bool Profile = false>
class block_freq_index {
   public:
    template <typename Codec>
    block_freq_index(Codec codec) : m_size(0), m_codec(std::move(codec))
    {
    }

    static auto from_type(std::string_view type) -> std::unique_ptr<block_freq_index>
    {
        if (auto codec = BlockCodec::from_type(type); codec) {
            return std::make_unique<block_freq_index>(*codec);
        } else {
            return nullptr;
        }
    }

    class builder {
       public:
        builder(uint64_t num_docs, global_parameters const &params, BlockCodec codec)
            : m_params(params), m_codec(std::move(codec))
        {
            m_num_docs = num_docs;
            m_endpoints.push_back(0);
        }

        template <typename DocsIterator, typename FreqsIterator>
        void add_posting_list(uint64_t n,
                              DocsIterator docs_begin,
                              FreqsIterator freqs_begin,
                              uint64_t /* occurrences */)
        {
            if (!n)
                throw std::invalid_argument("List must be nonempty");
            block_posting_list<Profile>::write(m_lists, n, docs_begin, freqs_begin, m_codec);
            m_endpoints.push_back(m_lists.size());
        }

        template <typename BlockDataRange>
        void add_posting_list(uint64_t n, BlockDataRange const &blocks)
        {
            if (!n)
                throw std::invalid_argument("List must be nonempty");
            block_posting_list<Profile>::write_blocks(m_lists, n, blocks);
            m_endpoints.push_back(m_lists.size());
        }

        template <typename BytesRange>
        void add_posting_list(BytesRange const &data)
        {
            m_lists.insert(m_lists.end(), std::begin(data), std::end(data));
            m_endpoints.push_back(m_lists.size());
        }

        void build(block_freq_index &sq)
        {
            sq.m_params = m_params;
            sq.m_size = m_endpoints.size() - 1;
            sq.m_num_docs = m_num_docs;
            sq.m_lists.steal(m_lists);

            bit_vector_builder bvb;
            compact_elias_fano::write(bvb,
                                      m_endpoints.begin(),
                                      sq.m_lists.size(),
                                      sq.m_size,
                                      m_params); // XXX
            bit_vector(&bvb).swap(sq.m_endpoints);
        }

       private:
        global_parameters m_params;
        size_t m_num_docs;
        std::vector<uint64_t> m_endpoints;
        std::vector<uint8_t> m_lists;
        BlockCodec m_codec;
    };

    size_t size() const { return m_size; }

    uint64_t num_docs() const { return m_num_docs; }

    typedef typename block_posting_list<Profile>::document_enumerator document_enumerator;

    document_enumerator operator[](size_t i) const
    {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(), m_size, m_params);

        auto endpoint = endpoints.move(i).second;
        return document_enumerator(m_lists.data() + endpoint, num_docs(), m_codec, i);
    }

    void warmup(size_t i) const
    {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(), m_size, m_params);

        auto begin = endpoints.move(i).second;
        auto end = m_lists.size();
        if (i + 1 != size()) {
            end = endpoints.move(i + 1).second;
        }

        volatile uint32_t tmp;
        for (size_t i = begin; i != end; ++i) {
            tmp = m_lists[i];
        }
        (void)tmp;
    }

    void swap(block_freq_index &other)
    {
        std::swap(m_params, other.m_params);
        std::swap(m_size, other.m_size);
        m_endpoints.swap(other.m_endpoints);
        m_lists.swap(other.m_lists);
    }

    template <typename Visitor>
    void map(Visitor &visit)
    {
        visit(m_params, "m_params")(m_size, "m_size")(m_num_docs, "m_num_docs")(
            m_endpoints, "m_endpoints")(m_lists, "m_lists");
    }

   private:
    global_parameters m_params;
    size_t m_size;
    size_t m_num_docs;
    bit_vector m_endpoints;
    mapper::mappable_vector<uint8_t> m_lists;
    BlockCodec m_codec;
};

template <typename T>
[[nodiscard]] auto make_block_freq_index() -> block_freq_index<>
{
    return block_freq_index(T{});
}

} // namespace pisa
