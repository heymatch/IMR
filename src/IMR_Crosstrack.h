#ifndef IMR_CROSSTRACK_H
#define IMR_CROSSTRACK_H

#include <cstdlib>
#include <vector>
#include <fstream>
#include <iostream>

namespace IMR_Crosstrack{
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
            Update_Method update_method = Update_Method::IN_PLACE
        ) : 
        LBA_TOTAL(lba_total),
        TRACK_NUM(track_num),
        SECTORS_PER_BOTTOM_TRACK(sectors_per_bottom_track),
        SECTORS_PER_TOP_TRACK(sectors_per_top_track),
        TOTAL_BOTTOM_TRACK(total_bottom_track),
        TOTAL_TOP_TRACK(total_top_track),
        UPDATE_METHOD(update_method)
        {}

        const size_t LBA_TOTAL;
        const size_t TRACK_NUM;
        const size_t SECTORS_PER_BOTTOM_TRACK;
        const size_t SECTORS_PER_TOP_TRACK;
        const size_t TOTAL_BOTTOM_TRACK;
        const size_t TOTAL_TOP_TRACK;

        const Update_Method UPDATE_METHOD;
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

    std::vector<size_t> LBA_to_PBA;
    std::vector<size_t> PBA_to_LBA;
    std::vector<bool> track_written;

    size_t write_position;

    // * initialize and run

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void evaluation();

    // * for Partition

    void read(const Trace &request, std::ostream &output_file);
    void inplace_crosstrack_write(const Trace &request, std::ostream &output_file);
    void outplace_crosstrack_write(const Trace &request, std::ostream &output_file);

    // * for output

    void write_requests_file(const std::vector<Trace> &requests, std::ostream &out_file);

    // * sub-functions

    inline size_t get_PBA(const size_t &LBA) { return LBA_to_PBA[LBA]; }
    inline void set_LBA_PBA(const size_t &LBA, const size_t &PBA) { LBA_to_PBA[LBA] = PBA; PBA_to_LBA[PBA] = LBA; }
    size_t get_track(const size_t &PBA);
    size_t get_track_head(const size_t &track);

    // * BOTTOM is even, TOP is odd
    inline bool isTop(const size_t &track) { return track % 2; }
}

#endif