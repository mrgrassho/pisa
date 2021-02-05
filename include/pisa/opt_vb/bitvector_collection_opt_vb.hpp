#pragma once

#include "bit_vector.hpp"

#include "./compact_elias_fano_opt_vb.hpp"

namespace pvb {

    class bitvector_collection_opt_vb {
    public:
        bitvector_collection_opt_vb()
            : m_size(0)
        {}

        class builder {
        public:
            builder(global_parameters_opt_vb const& params)
                : m_params(params)
            {
                m_endpoints.push_back(0);
            }

            void append(pisa::bit_vector_builder& bvb)
            {
                m_bitvectors.append(bvb);
                m_endpoints.push_back(m_bitvectors.size());
            }

            void build(bitvector_collection_opt_vb& sq)
            {
                sq.m_size = m_endpoints.size() - 1;
                pisa::bit_vector(&m_bitvectors).swap(sq.m_bitvectors);

                pisa::bit_vector_builder bvb;
                compact_elias_fano_opt_vb::write(bvb, m_endpoints.begin(),
                                          m_bitvectors.size(), sq.m_size,
                                          m_params);
                pisa::bit_vector(&bvb).swap(sq.m_endpoints);
            }

            size_t size() const
            {
                return m_bitvectors.size();
            }

        private:
            global_parameters_opt_vb m_params;
            std::vector<uint64_t> m_endpoints;
            pisa::bit_vector_builder m_bitvectors;
        };

        size_t size() const
        {
            return m_size;
        }

        pisa::bit_vector const& bits() const
        {
            return m_bitvectors;
        }

        pisa::bit_vector::enumerator
        get(global_parameters_opt_vb const& params, size_t i) const
        {
            assert(i < size());
            compact_elias_fano_opt_vb::enumerator endpoints(m_endpoints, 0,
                                                     m_bitvectors.size(), m_size,
                                                     params);

            auto endpoint = endpoints.move(i).second;
            return pisa::bit_vector::enumerator(m_bitvectors, endpoint);
        }

        void swap(bitvector_collection_opt_vb& other)
        {
            std::swap(m_size, other.m_size);
            m_endpoints.swap(other.m_endpoints);
            m_bitvectors.swap(other.m_bitvectors);
        }

        template <typename Visitor>
        void map(Visitor& visit)
        {
            visit
                (m_size, "m_size")
                (m_endpoints, "m_endpoints")
                (m_bitvectors, "m_bitvectors")
                ;
        }

    private:
        size_t m_size;
        pisa::bit_vector m_endpoints;
        pisa::bit_vector m_bitvectors;
    };
}
