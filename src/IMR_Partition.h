#ifndef IMR_PARTITION_H
#define IMR_PARTITION_H

#include <cstdlib>
#include <vector>
#include <fstream>
#include <iostream>

namespace IMR_Partition{
    namespace Evaluation{
        size_t cache_partition_times;
        size_t load_partition_map_times;
        size_t write_back_partition_map_times;
        size_t cold_write_times;
        size_t cold_update_times;
        size_t clean_buffer_times;
        size_t access_during_clean_buffer;
    }

    struct Options{
        Options(
            size_t lba_total = 103219550,
            size_t track_num = 53001,
            size_t sectors_per_bottom_track = 2050,
            size_t sectors_per_top_track = 1845,
            size_t total_bottom_track = 26501,
            size_t total_top_track = 26500,

            size_t hot_data_def_size = 64,
            size_t partition_size = 500,
            size_t max_partition_size = 2000,
            size_t buffer_size = 21,
            size_t mapping_cache_size = 10,
            size_t segment_size = 41
        ) : 
        LBA_TOTAL(lba_total),
        TRACK_NUM(track_num),
        SECTORS_PER_BOTTOM_TRACK(sectors_per_bottom_track),
        SECTORS_PER_TOP_TRACK(sectors_per_top_track),
        TOTAL_BOTTOM_TRACK(total_bottom_track),
        TOTAL_TOP_TRACK(total_top_track),

        HOT_DATA_DEF_SIZE(hot_data_def_size),
        PARTITION_SIZE(partition_size),
        MAX_PARTITION_SIZE(max_partition_size),
        BUFFER_SIZE(buffer_size),
        MAPPING_CACHE_SIZE(mapping_cache_size),
        SEGMENT_SIZE(segment_size),

        SECTORS_OF_BUFFER((BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTOM_TRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOP_TRACK))
        {

        }

        const size_t LBA_TOTAL;
        const size_t TRACK_NUM;
        const size_t SECTORS_PER_BOTTOM_TRACK;
        const size_t SECTORS_PER_TOP_TRACK;
        const size_t TOTAL_BOTTOM_TRACK;
        const size_t TOTAL_TOP_TRACK;
        
        const size_t HOT_DATA_DEF_SIZE;
        const size_t PARTITION_SIZE;
        const size_t MAX_PARTITION_SIZE;
        const size_t BUFFER_SIZE;
        const size_t MAPPING_CACHE_SIZE;
        const size_t SEGMENT_SIZE;

        const size_t SECTORS_OF_BUFFER;
    };

    static Options options;

    struct Trace{
        Trace(
            size_t time = 0,
            char iotype = 0,
            size_t address = 0,
            size_t size = 0,
            size_t device = 0
        ) :
        time(time),
        iotype(iotype),
        address(address),
        size(size),
        device(device)
        {}

        size_t time;
        char iotype;
        size_t address;
        size_t size;
        size_t device;
    };

    std::ostream& operator<<(std::ostream &out, const Trace &right){
        out 
        << right.time << "\t" 
        << right.device << "\t"
		<< right.address << "\t" 
        << right.size << "\t" 
        << right.iotype << "\n";

        return out;
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
            return buffer_write_position == buffer_head + options.SECTORS_OF_BUFFER;
        }

        size_t cold_used;
        bool cold_extending;
    };

    std::vector<size_t> LBA_to_PBA;
    std::vector<size_t> PBA_to_LBA;
    std::vector<bool> track_written;
    std::vector<Partition> partitions;
    std::vector<int> mapping_cache;

    size_t outplace_write_position;
    size_t cold_write_position;
    size_t hot_write_position;

    // * initialize and run

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void evaluation();

    // * for Partition

    void read(const Trace &request, std::ostream &output_file);
    void hot_data_write(const Trace &request, std::ostream &output_file);
    void cold_data_write(const Trace &request, std::ostream &output_file);
    void write_buffer(Partition &current_partition, Trace &write_request, std::ostream &output_file);
    void cache_partition();

    // * for output

    void write_requests_file(const std::vector<Trace> &requests, std::ostream &out_file);

    // * sub-functions

    size_t get_partition_position();
    inline size_t get_PBA(const size_t &LBA) { return LBA_to_PBA[LBA]; }
    inline size_t get_LBA(const size_t &PBA) { return PBA_to_LBA[PBA]; }
    inline void set_LBA_PBA(const size_t &LBA, const size_t &PBA) { LBA_to_PBA[LBA] = PBA; PBA_to_LBA[PBA] = LBA; }
    size_t get_track(const size_t &PBA);
    size_t get_track_head(const size_t &track);

    // * BOTTOM is even, TOP is odd
    inline bool isTop(const size_t &track) { return track % 2; }
}

#endif