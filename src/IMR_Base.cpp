#include "IMR_Base.h"

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

    options.TOTAL_TRACKS = options.TOTAL_BOTTOM_TRACK + options.TOTAL_TOP_TRACK;
    options.TOTAL_SECTORS = options.SECTORS_PER_BOTTOM_TRACK * options.TOTAL_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK * options.TOTAL_TOP_TRACK;

    // *
    #ifdef VECTOR_MAPPING
    LBA_to_PBA.resize(options.TOTAL_SECTORS, -1);
    PBA_to_LBA.resize(options.TOTAL_SECTORS, -1);
    #endif
    eval.track_load_count.resize(options.TOTAL_TRACKS);
}

void IMR_Base::evaluation(std::string &evaluation_file){
    evaluation_stream.open(evaluation_file + "eval");
    distribution_stream.open(evaluation_file + "dist");

    evaluation_stream << std::fixed << std::setprecision(2);

    evaluation_stream << "=== Options ==="                                              << "\n";
    evaluation_stream << "options.UPDATE_METHOD: "      << options.UPDATE_METHOD        << "\n";
    evaluation_stream << "options.TOTAL_BOTTOM_TRACK: " << options.TOTAL_BOTTOM_TRACK   << "\n";
    evaluation_stream << "options.TOTAL_TOP_TRACK: "    << options.TOTAL_TOP_TRACK      << "\n";
    evaluation_stream << "options.TRACK_NUM: "          << options.TOTAL_TRACKS         << "\n";
    evaluation_stream << "options.HOT_DATA_DEF_SIZE: "  << options.HOT_DATA_DEF_SIZE    << "\n";
    evaluation_stream << "options.APPEND_PARTS: "       << options.APPEND_PARTS         << "\n";
    evaluation_stream << "options.APPEND_COLD_SIZE: "   << options.APPEND_COLD_SIZE     << "\n";

    evaluation_stream << "=== Trace Information ==="                                    << "\n";
    evaluation_stream << "eval.trace_requests: "        << eval.trace_total_requests          << "\n";
    evaluation_stream << "eval.append_trace_size: "     << eval.append_trace_size       << "\n";
    evaluation_stream << "eval.max_LBA: "               << eval.max_LBA                 << "\n";

    evaluation_stream << "=== Evaluation ==="                                           << "\n";
    #ifdef MAP_MAPPING
    evaluation_stream << "Total Sector: "               << options.TOTAL_SECTORS        << "\n";
    evaluation_stream << "Total Sector Used: "          << get_LBA_size()               << "\n";
    evaluation_stream << "Total Sector Used Ratio: "    << ((double) get_LBA_size() / (double) options.TOTAL_SECTORS) * 100.0 << "%" << "\n";
    #endif
    #ifdef VECTOR_MAPPING
    evaluation_stream << "Total Sector Used: "            << eval.total_sector_used << " / " << options.TOTAL_SECTORS                             << "\n";
    evaluation_stream << "Total Sector Used Ratio: "      << ((double) eval.total_sector_used / (double) options.TOTAL_SECTORS) * 100.0 << "%"    << "\n";
    #endif

    size_t total_track_used = 0;
    for(size_t i = 0; i < track_written.size(); ++i){
        if(track_written[i]) ++total_track_used;
    }
    size_t total_tracks = options.TOTAL_TOP_TRACK + options.TOTAL_BOTTOM_TRACK;
    evaluation_stream << "Total Track: "                        << total_tracks                 << "\n";
    evaluation_stream << "Total Track Used: "                   << total_track_used             << "\n";
    evaluation_stream << "Total Track Used Ratio: "             << ((double) total_track_used / (double) total_tracks) * 100.0 << "%" << "\n";

    evaluation_stream << "Update Counts (request): "            << eval.update_times            << "\n";
    evaluation_stream << "eval.read_seek_distance_total: "      << eval.read_seek_distance_total            << "\n";


    distribution_stream << "Update Distribution: " << "\n";
    for(size_t i = 0; i < 11; ++i){
        if(i == 10)
            distribution_stream << "update_size_up"  << "\n";
        else
            distribution_stream << "update_size_" << (1 << i) << ",";
    }
    for(size_t i = 0; i < 11; ++i){
        distribution_stream << eval.update_dist[i];
        if(i == 10) distribution_stream << "\n"; else distribution_stream << ",";
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
        size_t PBA = get_PBA(request.address + i);
		if(PBA == -1 && write_request.size == 0){
            write_request.size += 1;
            write_request.address = request.address + i;
        }
        else if(PBA == -1){
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

    size_t last_track = get_track(get_PBA(request.address));
    for (int i = 0; i < request.size; i++) {
        size_t PBA = get_PBA(request.address + i);
		if(PBA == -1){
            throw "<error> read at not written";
        }

        size_t cur_track = get_track(PBA);
        if(cur_track != last_track){
            if(cur_track < last_track)
                eval.read_seek_distance_total += last_track - cur_track;
            else
                eval.read_seek_distance_total += cur_track - last_track;
            last_track = cur_track;
        }

        requests.push_back(
            Request(
                request.timestamp,
                'R',
                PBA,
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
    size_t min_addr = -1;
    size_t max_addr = 0;

    size_t line_counter = 1;
    std::string line;

    size_t trace_requests = 0;
    size_t append_trace_size = 0;

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
            trace.timestamp = (double) (std::stod(fields[0]) / 10000000.0 - 11644473600.0);
            trace.iotype = fields[3][0];
            trace.device = 0;
            trace.address = std::stoull(fields[4]);
            trace.size = std::stoull(fields[5]);
        }

        if(trace.iotype == '1'){
            trace.iotype = 'R';
        }
        else if(trace.iotype == '0'){
            trace.iotype = 'W';
        }

        // std::clog << std::fixed << trace;

        // if(!trace_stream.eof()){
        //     std::cerr << "<error> trace fail at " << line_counter << std::endl;
        //     exit(EXIT_FAILURE);
        // }
        
        trace.timestamp *= 1000.0;
        trace.address /= 512;
        trace.size /= 512;
        
        min_addr = std::min(min_addr, trace.address);
        max_addr = std::max(max_addr, trace.address);

        order_queue.push(trace);

        trace_requests += 1;
        append_trace_size += trace.size;
        eval.max_request_write_size = std::max(eval.max_request_write_size, trace.size);
    }

    eval.trace_total_requests = trace_requests;
    eval.append_trace_size = append_trace_size;
    eval.shifting_address = min_addr;
    eval.max_LBA = max_addr - min_addr;

    std::clog << "<debug> eval.max_LBA: " << eval.max_LBA << std::endl;
    std::clog << "<debug> eval.max_request_write_size: " << eval.max_request_write_size << std::endl;
}

void IMR_Base::write_file(std::ostream &output_file){
    while(!order_queue.empty()){
        output_file << order_queue.top();
        order_queue.pop();
    }
}