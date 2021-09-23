#include "IMR_Base.h"

std::priority_queue<Request> order_queue;

void IMR_Base::read(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    for (int i = 0; i < request.size; i++) {
        size_t readPBA = get_PBA(request.address + i);
		if (readPBA == -1){
            write(request, output_file);
            readPBA = get_PBA(request.address + i);
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
    std::string line;
    // * remove header
    std::getline(input_file, line);

    while(std::getline(input_file, line)){
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