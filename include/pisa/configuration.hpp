#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <thread>

#include "boost/lexical_cast.hpp"

namespace pisa {

class configuration {
  public:
    static configuration const& get()
    {
        static configuration instance;
        return instance;
    }

    uint64_t fix_cost{64};////////
    size_t log_partition_size{7};////////
    size_t quantization_bits{8};
    bool heuristic_greedy{false};

  private:
    configuration()
    {
        fillvar("PISA_HEURISTIC_GREEDY", heuristic_greedy);
        fillvar("PISA_QUANTIZTION_BITS", quantization_bits);
        fillvar("DS2I_FIXCOST", fix_cost);///////////
        fillvar("DS2I_LOG_PART", log_partition_size);///////////
    }

    template <typename T>
    void fillvar(const char* envvar, T& var)
    {
        const char* val = std::getenv(envvar);
        if (val && strlen(val)) {
            var = boost::lexical_cast<T>(val);
        }
    }
};

}  // namespace pisa
