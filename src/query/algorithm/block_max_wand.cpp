#include "cursor/scored_cursor.hpp"
#include "index_types.hpp"
#include "macro.hpp"
#include "query/algorithm/block_max_wand_query.hpp"

namespace pisa {

#define PISA_BLOCK_MAX_WAND_EXECUTOR(SCORER, INDEX, WAND)                               \
    template QueryExecutor block_max_wand_executor(BOOST_PP_CAT(INDEX, _index) const &, \
                                                   wand_data<WAND> const &,             \
                                                   SCORER<wand_data<WAND>> const &,     \
                                                   int);

#define LOOP_BODY(R, DATA, T)                                                          \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, bm25, T, wand_data_raw)        \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, dph, T, wand_data_raw)         \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, pl2, T, wand_data_raw)         \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, qld, T, wand_data_raw)         \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, bm25, T, wand_data_compressed) \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, dph, T, wand_data_compressed)  \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, pl2, T, wand_data_compressed)  \
    PISA_DAAT_BLOCK_MAX_ALGORITHM(block_max_wand_query, qld, T, wand_data_compressed)  \
    PISA_BLOCK_MAX_WAND_EXECUTOR(bm25, T, wand_data_raw)                               \
    PISA_BLOCK_MAX_WAND_EXECUTOR(dph, T, wand_data_raw)                                \
    PISA_BLOCK_MAX_WAND_EXECUTOR(pl2, T, wand_data_raw)                                \
    PISA_BLOCK_MAX_WAND_EXECUTOR(qld, T, wand_data_raw)                                \
    PISA_BLOCK_MAX_WAND_EXECUTOR(bm25, T, wand_data_compressed)                        \
    PISA_BLOCK_MAX_WAND_EXECUTOR(dph, T, wand_data_compressed)                         \
    PISA_BLOCK_MAX_WAND_EXECUTOR(pl2, T, wand_data_compressed)                         \
    PISA_BLOCK_MAX_WAND_EXECUTOR(qld, T, wand_data_compressed)
/**/
BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, PISA_INDEX_TYPES);
#undef LOOP_BODY
#undef PISA_BLOCK_MAX_WAND_EXECUTOR

} // namespace pisa