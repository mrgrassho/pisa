#pragma once

#include <string>
#include <vector>

#include <gsl/span>

#include "macro.hpp"
#include "query/queries.hpp"

namespace pisa {

struct ranked_or_query {

    ranked_or_query(uint64_t k)
        : m_topk(k){}

    template <typename Cursor>
    uint64_t operator()(gsl::span<Cursor> cursors, uint64_t max_docid);

    std::vector<std::pair<float, uint64_t>> const &topk() const { return m_topk.topk(); }

   private:
    topk_queue m_topk;
};

template <typename Cursor>
uint64_t ranked_or_query::operator()(gsl::span<Cursor> cursors, uint64_t max_docid)
{
    m_topk.clear();
    if (cursors.empty()) {
        return 0;
    }
    uint64_t cur_doc =
        std::min_element(cursors.begin(), cursors.end(), [](Cursor const &lhs, Cursor const &rhs) {
            return lhs.docs_enum.docid() < rhs.docs_enum.docid();
        })->docs_enum.docid();

    while (cur_doc < max_docid) {
        float score = 0;
        uint64_t next_doc = max_docid;
        for (size_t i = 0; i < cursors.size(); ++i) {
            if (cursors[i].docs_enum.docid() == cur_doc) {
                score +=
                    cursors[i].scorer(cursors[i].docs_enum.docid(), cursors[i].docs_enum.freq());
                cursors[i].docs_enum.next();
            }
            if (cursors[i].docs_enum.docid() < next_doc) {
                next_doc = cursors[i].docs_enum.docid();
            }
        }

        m_topk.insert(score, cur_doc);
        cur_doc = next_doc;
    }

    m_topk.finalize();
    return m_topk.topk().size();
}

template <typename Index, typename Scorer>
[[nodiscard]] auto ranked_or_executor(Index const &index, Scorer const &scorer, int k)
    -> QueryExecutor
{
    return [&, run = ranked_or_query(k)](Query query) mutable {
        auto cursors = make_scored_cursors(index, scorer, query);
        run(gsl::make_span(cursors), index.num_docs());
        return run.topk();
    };
}

template <typename Index, typename TermScorer>
struct scored_cursor;

#define PISA_RANKED_OR_EXECUTOR(SCORER, INDEX, WAND)  \
    extern template QueryExecutor ranked_or_executor( \
        BOOST_PP_CAT(INDEX, _index) const &, SCORER<wand_data<WAND>> const &, int);

#define LOOP_BODY(R, DATA, T)                                                  \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, bm25, T, wand_data_raw)        \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, dph, T, wand_data_raw)         \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, pl2, T, wand_data_raw)         \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, qld, T, wand_data_raw)         \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, bm25, T, wand_data_compressed) \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, dph, T, wand_data_compressed)  \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, pl2, T, wand_data_compressed)  \
    PISA_DAAT_ALGORITHM_EXTERN(ranked_or_query, qld, T, wand_data_compressed)  \
    PISA_RANKED_OR_EXECUTOR(bm25, T, wand_data_raw)                            \
    PISA_RANKED_OR_EXECUTOR(dph, T, wand_data_raw)                             \
    PISA_RANKED_OR_EXECUTOR(pl2, T, wand_data_raw)                             \
    PISA_RANKED_OR_EXECUTOR(qld, T, wand_data_raw)                             \
    PISA_RANKED_OR_EXECUTOR(bm25, T, wand_data_compressed)                     \
    PISA_RANKED_OR_EXECUTOR(dph, T, wand_data_compressed)                      \
    PISA_RANKED_OR_EXECUTOR(pl2, T, wand_data_compressed)                      \
    PISA_RANKED_OR_EXECUTOR(qld, T, wand_data_compressed)                      \
/**/
BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, PISA_INDEX_TYPES);
#undef LOOP_BODY
#undef PISA_RANKED_OR_EXECUTOR

}  // namespace pisa
