#ifndef IMR_PARTITION_H
#define IMR_PARTITION_H

#include "IMR_Base.h"

namespace IMR_Partition{
    namespace Evaluation{
        static size_t cache_partition_times;
        static size_t load_partition_map_times;
        static size_t write_back_partition_map_times;
        static size_t cold_write_times;
        static size_t cold_update_times;
        static size_t clean_buffer_times;
        static size_t access_during_clean_buffer;
    }

    struct Partition{
        Partition() :
        cold_extending(false)
        {}

        size_t head;
        size_t size;
        size_t hot_size;
        size_t map_size; //?

        size_t buffer_head;
        size_t buffer_write_position;
        std::vector<size_t> buffer_PBA;

        bool isBufferFull(){
            return buffer_write_position == buffer_head + IMR_Base::options.SECTORS_OF_BUFFER;
        }

        size_t cold_used;
        bool cold_extending;
    };


    static std::vector<Partition> partitions;
    static std::vector<int> mapping_cache;

    static size_t outplace_write_position;
    static size_t cold_write_position;
    static size_t hot_write_position;

    // * initialize and run

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void evaluation();

    // * for Partition

    void read(const IMR_Base::Request &request, std::ostream &output_file);
    void hot_data_write(const IMR_Base::Request &request, std::ostream &output_file);
    void cold_data_write(const IMR_Base::Request &request, std::ostream &output_file);
    void write_buffer(Partition &current_partition, const IMR_Base::Request &write_request, std::ostream &output_file);
    void cache_partition();

    static inline size_t get_partition_position(const size_t &track){
        long long N = 0;
        long long total = 0;
        for (N = 0; N < partitions.size(); N++) {
            if (track >= total && track < (total + partitions[N].size))
                return N;
            total += partitions[N].size;
        }
        return 0;
    }

}

#endif