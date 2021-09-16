#ifndef IMR_SEQUENTIAL_H
#define IMR_SEQUENTIAL_H

#include "IMR_Base.h"
using IMR_Base::Request;
using IMR_Base::LBA_to_PBA;
using IMR_Base::PBA_to_LBA;
using IMR_Base::track_written;

namespace IMR_Sequential{
    static size_t write_position;

    // * initialize options and default values
    void initialize(std::ifstream &);

    void run(std::ifstream &, std::ofstream &);
    void evaluation();

    // * main functions

    void inplace_sequential_write(const Request &request, std::ostream &output_file);
    void outplace_sequential_write(const Request &request, std::ostream &output_file);
}

#endif