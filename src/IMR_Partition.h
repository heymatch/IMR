#ifndef IMR_PARTITION_H
#define IMR_PARTITION_H

#include "IMR_Base.h"

class IMR_Partition : public IMR_Base{
private:
    bool eval_reload_partition(const size_t);

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
            hot_used_track(0),
            cache_load_counts(0)
            {}

            Partition(const IMR_Partition &, const size_t, const IMR_Partition::Partition::HOT_ZONE);

            size_t get_init_hot_write_position();
            size_t get_init_cold_write_position();

            size_t partition_head_track;
            size_t partition_alloc_track;

            size_t hot_alloc_track;
            size_t hot_head_track; 
            size_t hot_used_track = 0;
            size_t hot_head_sector;
            size_t hot_reservation_sector;
            size_t hot_end_sector;

            size_t buffer_alloc_track = 21;
            size_t buffer_head_track;
            size_t buffer_used_track = 0;
            size_t buffer_extend_track = 0;
            size_t buffer_head_sector;
            size_t buffer_tail_sector;
            size_t buffer_write_position;
            size_t buffer_writeback_count = 0;

            size_t mapping_alloc_track = 1;

            size_t cold_used_track = 0;
            size_t cold_head_track;
            size_t cold_head_sector;
            size_t cold_reservation_sector;
            size_t cold_end_sector;
            
            size_t used_track_size = 0;
            size_t mapping_track;
            size_t id;

            
            std::vector<size_t> buffer_PBA;

            inline bool isBufferFull(const Options &options){
                return buffer_write_position <= buffer_tail_sector;
            }

            inline bool isHotReservation(const IMR_Partition &IMR){
                return IMR.hot_write_position >= hot_reservation_sector;
            }
            inline bool isHotFull(const IMR_Partition &IMR){
                return IMR.hot_write_position >= hot_end_sector;
            }
            inline bool isHotWriteFull(const IMR_Partition &IMR, const size_t write_size){
                return IMR.hot_write_position + write_size >= hot_end_sector;
            }

            inline bool isColdReservation(const IMR_Partition &IMR){
                return IMR.cold_write_position >= cold_reservation_sector;
            }
            inline bool isColdFull(const IMR_Partition &IMR){
                return IMR.cold_write_position >= cold_end_sector;
            }
            inline bool isColdWriteFull(const IMR_Partition &IMR, const size_t write_size){
                return IMR.cold_write_position + write_size >= cold_end_sector;
            }

            bool cold_extending = false;
            
            
            size_t cache_load_counts = 0;
            size_t partition_reload_begin_sector_count = 0;
            size_t partition_reload_end_sector_count = 0;
            size_t partition_reload_request_count = 0;
    };

    IMR_Partition() : IMR_Base(){

    }

    std::vector<Partition> partitions;
    std::deque<int> mapping_cache;

    size_t latest_partition;
    size_t cold_write_position;
    size_t hot_write_position;
    bool cold_reservation_detection = false;
    bool hot_reservation_detection = false;

    // * initialize and run

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void request(Request &);
    void evaluation(std::string &);

    void write(const Request &, std::ostream &);
    void write_append(const Request &, std::ostream &);

    // * for Partition

    void read(const Request &request, std::ostream &output_file);
    void hot_request(const Request &request, std::ostream &output_file);
    void hot_write(const Request &request, std::ostream &output_file);
    void hot_update(const Request &request, std::ostream &output_file);
    void cold_request(const Request &request, std::ostream &output_file);
    void cold_write(const Request &request, std::ostream &output_file);
    void cold_update(const Request &request, std::ostream &output_file);
    void buffer_update(const Request &request, std::ostream &output_file);
    void buffer_writeback(Partition &current_partition, const Request &write_request, std::ostream &output_file);
    void cache_partition(const Request &request, const size_t &partition_number, std::ostream &);

    inline size_t get_partition_position(const size_t &track){
        size_t i = 0;
        size_t total = 0;
        for(size_t i = 0; i < partitions.size(); ++i) {
            size_t used_tracks = partitions[i].hot_alloc_track + partitions[i].buffer_alloc_track + 1 + partitions[i].cold_used_track;
            if(partitions[i].partition_head_track <= track && track < partitions[i].partition_head_track + partitions[i].partition_alloc_track){
                return i;
            }
        }
        return 0;
    }


    
};

#endif