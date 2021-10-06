#ifndef IMR_BASE_H
#define IMR_BASE_H

#include <cstdlib>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <iomanip>

enum Update_Method{
    IN_PLACE,
    OUT_PLACE
};

struct Options{
    Options(
        size_t lba_total = 103219550,
        size_t track_num = 53001,
        size_t sectors_per_bottom_track = 2050,
        size_t sectors_per_top_track = 1845,
        size_t total_bottom_track = 26501,
        size_t total_top_track = 26500,

        Update_Method update_method = Update_Method::IN_PLACE,

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

    UPDATE_METHOD(update_method),

    HOT_DATA_DEF_SIZE(hot_data_def_size),
    PARTITION_SIZE(partition_size),
    MAX_PARTITION_SIZE(max_partition_size),
    BUFFER_SIZE(buffer_size),
    MAPPING_CACHE_SIZE(mapping_cache_size),
    SEGMENT_SIZE(segment_size),

    SECTORS_OF_BUFFER((BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTOM_TRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOP_TRACK))
    {}

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

    Update_Method UPDATE_METHOD;
};

static Options options;

struct Request{
    Request(
        double time = 0,
        char iotype = 0,
        size_t address = 0,
        size_t size = 0,
        size_t device = 0
    ) :
    timestamp(time),
    iotype(iotype),
    address(address),
    size(size),
    device(device)
    {}

    bool operator<(const Request &right) const{
        return !(timestamp < right.timestamp);
    }

    double timestamp;
    double response;
    char iotype;
    size_t address;
    size_t size;
    size_t device;
};

extern std::priority_queue<Request> order_queue;

static std::ostream& operator<<(std::ostream &out, const Request &right){
    out 
    << right.timestamp << "\t" 
    << right.device << "\t"
    << right.address << "\t" 
    << right.size << "\t";

    if(right.iotype == 'W')
        out << '0' << "\n";
    else if(right.iotype == 'R')
        out << '1' << "\n";
    
    return out;
}

class IMR_Base{
public:
    std::vector<bool> track_written;

    // * main common functions

    virtual void initialize(std::ifstream &) = 0;
    virtual void run(std::ifstream &, std::ofstream &) = 0;
    virtual void evaluation(std::ofstream &) = 0;

    void read(const Request &request, std::ostream &output_file);
    virtual void write(const Request &, std::ostream &) = 0;

    // * for I/O
    void write_requests_file(const std::vector<Request> &requests, std::ostream &out_file);
    void read_file(std::istream &);
    void write_file(std::ostream &);

    // * sub-functions
    
    inline size_t get_PBA(const size_t &LBA) { return LBA_to_PBA.count(LBA) ? LBA_to_PBA[LBA] : -1; }
    inline size_t get_LBA(const size_t &PBA) { return PBA_to_LBA.count(PBA) ? PBA_to_LBA[PBA] : -1; }
    inline void set_LBA_PBA(const size_t &LBA, const size_t &PBA) { LBA_to_PBA[LBA] = PBA; PBA_to_LBA[PBA] = LBA; }
    
    // inline size_t get_PBA(const size_t &LBA) { 
    //     return LBA < LBA_to_PBA.size() ? LBA_to_PBA[LBA] : -1;
    // }
    // inline size_t get_LBA(const size_t &PBA) { 
    //     return PBA < PBA_to_LBA.size() ? PBA_to_LBA[PBA] : -1;
    // }
    // inline void set_LBA_PBA(const size_t &LBA, const size_t &PBA){
    //     if(PBA_to_LBA.size() < PBA)
    //         PBA_to_LBA.resize(PBA + 100, -1);

    //     LBA_to_PBA[LBA] = PBA;
    //     PBA_to_LBA[PBA] = LBA; 
    // }
    
    inline size_t get_track(const size_t &PBA){
        if (PBA == -1)
		return -1;
	
        size_t n = PBA / (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK);
        if (PBA - n * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK) > options.SECTORS_PER_BOTTOM_TRACK)
            return (2 * n + 1);
        else if (PBA - n * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK) > 0)
            return 2 * n;
        else
            return (2 * n - 1);
    }
    inline size_t get_track_head(const size_t &track){
        return 
            track % 2 ? 
            (track / 2 * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK)) + options.SECTORS_PER_BOTTOM_TRACK + 1 :
            (track / 2 * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK)) + 1;
    }

    // * BOTTOM is even, TOP is odd
    inline bool isTop(const size_t &track) { return track % 2; }

    // * For evaluation
    inline size_t get_LBA_size() { return LBA_to_PBA.size(); }

private:
    std::unordered_map<size_t, size_t> LBA_to_PBA;
    std::unordered_map<size_t, size_t> PBA_to_LBA;
    // std::vector<size_t> LBA_to_PBA;
    // std::vector<size_t> PBA_to_LBA;
};

#endif