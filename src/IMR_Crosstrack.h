#ifndef IMR_CROSSTRACK_H
#define IMR_CROSSTRACK_H

#include "IMR_Base.h"

namespace Evaluation{
    static size_t inplace_update_count = 0;
    static size_t outplace_update_count = 0;
    static size_t direct_update_bottom_count = 0;
    static size_t direct_update_top_count = 0;
}

class IMR_Crosstrack : public IMR_Base{
public:
    size_t write_position;

    // * initialize and run

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void evaluation(std::ofstream &);

    void write(const Request &, std::ostream &);

    // * main functions

    void inplace_crosstrack_write(const Request &request, std::ostream &output_file);
    void outplace_crosstrack_write(const Request &request, std::ostream &output_file);
};

#endif