#include "IMR_Base.h"

std::priority_queue<Request> order_queue;

void IMR_Base::initialize(std::ifstream &setting_file){
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
            else{
                throw "<error> wrong UPDATE_METHOD";
            }
            
        }
        else if(parameter == "TOTAL_BOTTOM_TRACK"){
            options.TOTAL_BOTTOM_TRACK = std::stoull(value);
        }
        else if(parameter == "TOTAL_TOP_TRACK"){
            options.TOTAL_TOP_TRACK = std::stoull(value);
        }
    }

    options.TRACK_NUM = options.TOTAL_BOTTOM_TRACK + options.TOTAL_TOP_TRACK;
}

void IMR_Base::evaluation(std::ofstream &evaluation_file){
    evaluation_file << std::fixed << std::setprecision(2);

    evaluation_file << "Options" << "\n";
    evaluation_file << "options.UPDATE_METHOD: " << options.UPDATE_METHOD << "\n";
    evaluation_file << "options.TOTAL_BOTTOM_TRACK: " << options.TOTAL_BOTTOM_TRACK << "\n";
    evaluation_file << "options.TOTAL_TOP_TRACK: " << options.TOTAL_TOP_TRACK << "\n";
    evaluation_file << "options.TRACK_NUM: " << options.TRACK_NUM << "\n";
    evaluation_file << "options.HOT_DATA_DEF_SIZE: " << options.HOT_DATA_DEF_SIZE << "\n";

    evaluation_file << "==========" << "\n";
    
    size_t total_sectors = 
        options.SECTORS_PER_BOTTOM_TRACK * options.TOTAL_BOTTOM_TRACK + 
        options.SECTORS_PER_TOP_TRACK * options.TOTAL_TOP_TRACK;
    evaluation_file << "Total Sector Used: " << get_LBA_size() << " / " << total_sectors << "\n";
    evaluation_file << "Total Sector Used Ratio: " << ((double) get_LBA_size() / (double) total_sectors) * 100.0 << "%" << "\n";

    size_t total_track_used = 0;
    for(size_t i = 0; i < track_written.size(); ++i){
        if(track_written[i]) ++total_track_used;
    }
    size_t total_tracks = options.TOTAL_TOP_TRACK + options.TOTAL_BOTTOM_TRACK;
    evaluation_file << "Total Track Used: " << total_track_used << " / " << total_tracks << "\n";
    evaluation_file << "Total Track Used Ratio: " << ((double) total_track_used / (double) total_tracks) * 100.0 << "%" << "\n";

    evaluation_file << "Update Times: " << eval.update_times << "\n";
    evaluation_file << "Update Distribution: " << "\n";
    for(size_t i = 0; i < 11; ++i){
        if(i == 10)
            evaluation_file << "update_size_up"  << "\n";
        else
            evaluation_file << "update_size_" << (1 << i) << ",";
    }
    for(size_t i = 0; i < 11; ++i){
        evaluation_file << eval.update_dist[i];
        if(i == 10) evaluation_file << "\n"; else evaluation_file << ",";
    }

}

void IMR_Base::read(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    Request write_request(
        request.timestamp,
        'W',
        request.address,
        0,
        request.device
    );
    for (int i = 0; i < request.size; i++) {
        size_t readPBA = get_PBA(request.address + i);
		if(readPBA == -1 && write_request.size == 0){
            write_request.size += 1;
            write_request.address = request.address + i;
        }
        else if(readPBA == -1){
            write_request.size += 1;
        }
        else if(write_request.size != 0){
            write(write_request, output_file);
            write_request.size = 0;
        }
	}

    if(write_request.size != 0){
        write(write_request, output_file);
    }

    for (int i = 0; i < request.size; i++) {
        size_t readPBA = get_PBA(request.address + i);
		if(readPBA == -1){
            throw "<error> read at not written";
        }

        requests.push_back(
            Request(
                request.timestamp,
                'R',
                readPBA,
                1,
                request.device
            )
        );
	}

    write_requests_file(requests, output_file);
}

void IMR_Base::write_requests_file(const std::vector<Request> &requests, std::ostream &output_file){
    Request transRequest(
        requests[0].timestamp,
        requests[0].iotype,
        requests[0].address,
        1,
        requests[0].device
    );

    size_t previous_PBA = -1;
    for(size_t i = 0; i < requests.size(); ++i){
        for(size_t j = 0; j < requests[i].size; ++j){
            if(previous_PBA != -1){
                if(
                    requests[i].address + j == previous_PBA + 1 &&
                    (get_track(requests[i].address + j) == get_track(previous_PBA) || requests[i].iotype == 'R') &&
                    requests[i].iotype == transRequest.iotype
                ){
                    transRequest.size += 1;
                }
                else{
                    // output_queue.push(transRequest);
                    output_file << transRequest;

                    transRequest.address = requests[i].address;
                    transRequest.iotype = requests[i].iotype;
                    transRequest.size = 1;
                }
            }

            previous_PBA = requests[i].address + j;
        }
    }

    // output_queue.push(transRequest);
    output_file << transRequest;
}

void IMR_Base::read_file(std::istream &input_file){
    size_t line_counter = 1;
    std::string line;
    // * remove header
    std::getline(input_file, line);

    while(std::getline(input_file, line)){
        line_counter += 1;
        std::stringstream split_stream(line);
        std::string split;
        std::stringstream trace_stream;

        Request trace;
        std::vector<std::string> fields;
        for(int field = 0; std::getline(split_stream, split, ','); ++field){
            fields.push_back(split);
        }

        if(options.TRACE_TYPE == Trace_Type::SYSTOR17){
            trace.timestamp = std::stod(fields[0]);
            trace.iotype = fields[2][0];
            trace.device = 0;
            trace.address = std::stoull(fields[4]);
            trace.size = std::stoull(fields[5]);
        }
        else if(options.TRACE_TYPE == Trace_Type::MSR){
            trace.timestamp = (double) (std::stoull(fields[0]) / 10000000 - 11644473600LL);
            trace.iotype = fields[3][0];
            trace.device = 0;
            trace.address = std::stoull(fields[4]);
            trace.size = std::stoull(fields[5]);
        }

        // std::clog << std::fixed << trace;

        // if(!trace_stream.eof()){
        //     std::cerr << "<error> trace fail at " << line_counter << std::endl;
        //     exit(EXIT_FAILURE);
        // }
        
        trace.timestamp *= 1000.0;
        trace.address /= 512;
        trace.size /= 512;

        order_queue.push(trace);
    }
}

void IMR_Base::write_file(std::ostream &output_file){
    while(!order_queue.empty()){
        output_file << order_queue.top();
        order_queue.pop();
    }
}