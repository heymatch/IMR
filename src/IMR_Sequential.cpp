#include "IMR_Sequential.h"
using namespace IMR_Base;

void IMR_Sequential::initialize(std::ifstream &setting_file){
    // * init options
    std::string line;
    while(std::getline(setting_file, line)){
        std::stringstream line_split(line);
        std::string parameter, value;
        std::getline(line_split, parameter, '=');
        std::getline(line_split, value);

        if(parameter == "UPDATE_METHOD"){
            if(value == "IN_PLACE"){
                options.UPDATE_METHOD = Update_Method::IN_PLACE;
            }
            else if(value == "OUT_PLACE"){
                options.UPDATE_METHOD = Update_Method::OUT_PLACE;
            }
        }

        // std::clog << "<log> parameter, value = " << parameter << ", " << value << std::endl; 
    }

    // * init 
	track_written.resize(options.TRACK_NUM, false);

    // * init write position
    write_position = 1;
}

void IMR_Sequential::run(std::ifstream &input_file, std::ofstream &output_file){
    std::string line;
    // * remove header
    std::getline(input_file, line);

    size_t processing = 0;

    while(std::getline(input_file, line)){
        if(processing % 1000000 == 0)
            std::clog << "<log> processing " << processing << std::endl;
        processing += 1;

        std::stringstream split_stream(line);
        std::string split;
        std::stringstream trace_stream;

        while(std::getline(split_stream, split, ','))
            trace_stream << split << " ";

        Request trace;
        for(int field = 0; field < 6; ++field){
            if(field == 0) trace_stream >> trace.timestamp;
            else if(field == 1) trace_stream >> trace.response;
            else if(field == 2) trace_stream >> trace.iotype;
            else if(field == 3) trace_stream >> trace.device;
            else if(field == 4) trace_stream >> trace.address;
            else if(field == 5) trace_stream >> trace.size;
        }
        
        trace.address /= 512;
        trace.size /= 512;

        // * read request
        if(trace.iotype == 'R' || trace.iotype == '1'){
            trace.iotype = 'R';
            
            read(trace, output_file);
        }
        // * write request
        else if(trace.iotype == 'W' || trace.iotype == '0'){
            trace.iotype = 'W';

            if(options.UPDATE_METHOD == Update_Method::IN_PLACE){
                inplace_sequential_write(trace, output_file);
            }
            else if(options.UPDATE_METHOD == Update_Method::OUT_PLACE){
                outplace_sequential_write(trace, output_file);
            }
        }

    }
}

void IMR_Sequential::inplace_sequential_write(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    size_t previous_write_PBA = -1;
    size_t previous_update_PBA = -1;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        if(PBA == -1){
            size_t current_write_track = get_track(write_position);

            if(isTop(current_write_track) || current_write_track == 0){
                Request writeRequest(
                    request.timestamp,
                    'W',
                    write_position,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;
            }
            else{
                size_t previous_write_track = get_track(previous_write_PBA);

                if(current_write_track != previous_write_track){
                    Request readRequest(
                        request.timestamp,
                        'R',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );

                    requests.push_back(readRequest);
                }

                Request writeBottomRequest(
                    request.timestamp,
                    'W',
                    write_position,
                    1,
                    request.device
                );
                requests.push_back(writeBottomRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;

                if(
                    i == request.size - 1 || 
                    current_write_track != get_track(write_position + 1) ||
                    get_PBA(LBA + 1) != -1
                ){
                    Request writeBackTopRequest(
                        request.timestamp,
                        'W',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );
                    requests.push_back(writeBackTopRequest);
                }
            }

            previous_write_PBA = write_position;
            write_position += 1;
        }
        else{
            // std::clog << "<debug> sequential in-place update" << std::endl;

            size_t current_update_track = get_track(PBA);

            if (isTop(current_update_track)) {
                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);
            }
            else {
                size_t previous_update_track = get_track(previous_update_PBA);

                // std::clog << "<debug> current_update_track: " << current_update_track << std::endl;
                // std::clog << "<debug> previous_update_track: " << previous_update_track << std::endl;
                if(current_update_track != previous_update_track){
                    // * read left top track
                    if(current_update_track >= 1 && track_written[current_update_track - 1]) {
                        Request readRequest(
                            request.timestamp,
                            'R',
                            get_track_head(current_update_track - 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );
                        requests.push_back(readRequest);
                    }
                    // * read right top track
                    if(current_update_track < options.TRACK_NUM && track_written[current_update_track + 1]) {
                        Request readRequest(
                            request.timestamp,
                            'R',
                            get_track_head(current_update_track + 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );
                        requests.push_back(readRequest);
                    }
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);

                if(
                    i == request.size - 1 ||
                    current_update_track != get_track(get_PBA(LBA + 1))
                ){
                    if(current_update_track >= 1 && track_written[current_update_track - 1]) {
                        Request writeBackLeftTopRequest(
                            request.timestamp,
                            'W',
                            get_track_head(current_update_track - 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );
                        requests.push_back(writeBackLeftTopRequest);
                    }
                    
                    if(current_update_track < options.TRACK_NUM && track_written[current_update_track + 1]) {
                        Request writeBackRightTopRequest(
                            request.timestamp,
                            'W',
                            get_track_head(current_update_track + 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );
                        requests.push_back(writeBackRightTopRequest);
                    }
                }

            }

            previous_update_PBA = PBA;
        }
    }
	
    // * output file
    write_requests_file(requests, output_file);
}

void IMR_Sequential::outplace_sequential_write(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    size_t previous_write_PBA = -1;
    size_t previous_update_PBA = -1;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        if(PBA == -1){
            size_t current_write_track = get_track(write_position);

            if(isTop(current_write_track) || current_write_track == 0){
                Request writeRequest(
                    request.timestamp,
                    'W',
                    write_position,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;
            }
            else{
                size_t previous_write_track = get_track(previous_write_PBA);

                if(current_write_track != previous_write_track){
                    Request readRequest(
                        request.timestamp,
                        'R',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );

                    requests.push_back(readRequest);
                }

                Request writeBottomRequest(
                    request.timestamp,
                    'W',
                    write_position,
                    1,
                    request.device
                );
                requests.push_back(writeBottomRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;

                if(
                    i == request.size - 1 ||
                    current_write_track != get_track(write_position + 1) ||
                    get_PBA(LBA + 1) != -1
                ){
                    Request writeBackTopRequest(
                        request.timestamp,
                        'W',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );
                    requests.push_back(writeBackTopRequest);
                }
            }

            previous_write_PBA = write_position;
            write_position += 1;
        }
        else{
            size_t current_update_track = get_track(PBA);

            if(isTop(current_update_track) || current_update_track == 0){
                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);
                set_LBA_PBA(LBA, PBA);
                track_written[current_update_track] = true;

                previous_update_PBA = PBA;
            }
            else {  //write bottom track
                size_t previous_update_track = get_track(previous_update_PBA);

                if(current_update_track != previous_update_track){
                    Request readRequest(
                        request.timestamp,
                        'R',
                        get_track_head(current_update_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );

                    requests.push_back(readRequest);
                }

                Request writeBottomRequest(
                    request.timestamp,
                    'W',
                    write_position,
                    1,
                    request.device
                );
                requests.push_back(writeBottomRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_update_track] = true;

                if(
                    i == request.size - 1
                    || current_update_track != get_track(write_position + 1)
                ){
                    Request writeBackTopRequest(
                        request.timestamp,
                        'W',
                        get_track_head(current_update_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );
                    requests.push_back(writeBackTopRequest);
                }

                previous_update_PBA = write_position;
                write_position += 1;
            }

        }
        
    }

    // * output file
    write_requests_file(requests, output_file);
}

void IMR_Sequential::evaluation(){

}