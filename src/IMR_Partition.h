#ifndef IMR_PARTITION_H
#define IMR_PARTITION_H

#include "IMR_Base.h"

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

    size_t buffer_head;
    size_t buffer_write_position;
    std::vector<size_t> buffer_PBA;

    bool isBufferFull(){
        return buffer_write_position == buffer_head + options.SECTORS_OF_BUFFER;
    }

    bool cold_extending;

    size_t cold_used;
    size_t load_count;
};

class IMR_Partition : public IMR_Base{
public:
    IMR_Partition() : IMR_Base(){

    }

    std::vector<Partition> partitions;
    std::deque<int> mapping_cache;

    size_t latest_partition;
    size_t cold_write_position;
    size_t hot_write_position;

    // * initialize and run

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void evaluation(std::ofstream &);

    void write(const Request &, std::ostream &);

    // * for Partition

    void read(const Request &request, std::ostream &output_file);
    void hot_data_write(const Request &request, std::ostream &output_file);
    void cold_data_write(const Request &request, std::ostream &output_file);
    void write_buffer(Partition &current_partition, const Request &write_request, std::ostream &output_file);
    void cache_partition(const Request &request, const size_t &partition_number, std::ostream &);

    inline size_t get_partition_position(const size_t &track){
        size_t N = 0;
        size_t total = 0;
        for (N = 0; N < partitions.size(); ++N) {
            if (track >= total && track < (total + partitions[N].size))
                return N;
            total += partitions[N].size;
        }
        return 0;
    }

};

#endif