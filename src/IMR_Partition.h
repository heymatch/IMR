#ifndef IMR_PARTITION_H
#define IMR_PARTITION_H

#include "IMR_Base.h"

class IMR_Partition : public IMR_Base{
public:
    class Partition{
        public:
            enum HOT_ZONE{
                INIT,
                TOO_LARGE,
                TOO_SMALL,
                APPROPRIATE
            };

            Partition() :
            cold_extending(false),
            used_hot_tracks(0),
            used_cold_tracks(0),
            cache_load_counts(0)
            {}

            Partition(const IMR_Partition &, const size_t, const IMR_Partition::Partition::HOT_ZONE);

            size_t get_init_hot_write_position();
            size_t get_init_cold_write_position();

            size_t head_track;
            size_t alloc_track_size;
            size_t alloc_hot_track_size;
            size_t buffer_track_size = 21;
            size_t cold_track_size = 0;
            size_t used_track_size = 0;
            size_t mapping_track;
            size_t id;

            size_t buffer_head_sector;
            size_t buffer_tail_sector;
            size_t buffer_write_position;
            size_t buffer_write_count = 0;
            std::vector<size_t> buffer_PBA;

            bool isBufferFull(const Options &options){
                return buffer_write_position <= buffer_tail_sector;
            }

            bool cold_extending = false;

            // * for evaluation

            size_t used_hot_tracks = 0;
            size_t used_cold_tracks = 0;
            size_t cache_load_counts = 0;
            size_t partition_reload_sector_count = 0;
    };

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
    void request(Request &);
    void evaluation(std::string &);

    void write(const Request &, std::ostream &);
    void write_append(const Request &, std::ostream &);

    // * for Partition

    void read(const Request &request, std::ostream &output_file);
    void hot_data_write(const Request &request, std::ostream &output_file);
    void hot_data_update(const Request &request, std::ostream &output_file);
    void cold_data_write(const Request &request, std::ostream &output_file);
    void cold_data_update(const Request &request, std::ostream &output_file);
    void write_buffer(Partition &current_partition, const Request &write_request, std::ostream &output_file);
    void cache_partition(const Request &request, const size_t &partition_number, std::ostream &);

    inline size_t get_partition_position(const size_t &track){
        size_t i = 0;
        size_t total = 0;
        for(size_t i = 0; i < partitions.size(); ++i) {
            size_t used_tracks = partitions[i].alloc_hot_track_size + partitions[i].buffer_track_size + 1 + partitions[i].cold_track_size;
           if(partitions[i].head_track <= track && track < partitions[i].head_track + partitions[i].alloc_track_size){
               return i;
           }
        }
        return 0;
    }


    
};

#endif