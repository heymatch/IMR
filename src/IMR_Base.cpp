#include "IMR_Base.h"

std::priority_queue<Request> order_queue;

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
        for(int field = 0; std::getline(split_stream, split, ','); ++field){
            trace_stream.clear();
            trace_stream << split;

            if(field == 0) trace_stream >> trace.timestamp;
            else if(field == 1) trace_stream >> trace.response;
            else if(field == 2) trace_stream >> trace.iotype;
            else if(field == 3) trace_stream >> trace.device;
            else if(field == 4) trace_stream >> trace.address;
            else if(field == 5) trace_stream >> trace.size;
        }

        // std::clog << std::fixed << trace;

        if(!trace_stream.eof()){
            std::cerr << "<error> trace fail at " << line_counter << std::endl;
            exit(EXIT_FAILURE);
        }
        
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