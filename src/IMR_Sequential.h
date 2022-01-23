#ifndef IMR_SEQUENTIAL_H
#define IMR_SEQUENTIAL_H

#include "IMR_Base.h"

class IMR_Sequential : public IMR_Base{
public:
    size_t write_position;

    // * initialize options and default values

    void initialize(std::ifstream &);
    void run(std::ifstream &, std::ofstream &);
    void evaluation(std::string &);

    void write(const Request &, std::ostream &);
    void write_append(const Request &, std::ostream &);

    // * main functions

    void inplace_sequential_write(const Request &request, std::ostream &output_file);
    void outplace_sequential_write(const Request &request, std::ostream &output_file);
};

#endif