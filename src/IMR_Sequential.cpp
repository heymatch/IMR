#include "IMR_Sequential.h"
using namespace IMR_Sequential;

void IMR_Sequential::initialize(std::ifstream &setting_file){
    // * init options

    // * init 
    LBA_to_PBA.resize(options.LBA_TOTAL + 1, -1);
	PBA_to_LBA.resize(options.LBA_TOTAL + 1, -1);
	track_written.resize(options.TRACK_NUM, false);

    // * init write position
    write_position = 0;
}

void IMR_Sequential::run(std::ifstream &input_file, std::ofstream &output_file){
    Trace trace;
    while(input_file >> trace.time >> trace.iotype >> trace.address >> trace.size){
        trace.time *= 1000;

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

void IMR_Sequential::read(const Trace &request, std::ostream &output_file){
    std::vector<Trace> read_requests;

    Trace transRequest;
    transRequest.time = request.time;
    transRequest.iotype = 'R';
    transRequest.address = request.address;
    transRequest.size = 1;
    transRequest.device = request.device;

    size_t prev_addr = -1;
    for (int i = 0; i < request.size; i++) {
        size_t next_addr = get_PBA(request.address + i);
		if (next_addr == -1)
            next_addr = request.address + i;

        if(prev_addr != -1){
            // * if address is sequential
            if(next_addr == prev_addr + 1) {
                transRequest.size += 1;
            }
            // * if address is not sequential
            else{		
                read_requests.push_back(transRequest);

                transRequest.address = next_addr;
                transRequest.size = 1;
            }
        }
		
		prev_addr = next_addr;
	}
    read_requests.push_back(transRequest);

    for(size_t i = 0; i < read_requests.size(); ++i){
        output_file << read_requests[i];
    }
}

void IMR_Sequential::inplace_sequential_write(const Trace &request, std::ostream &output_file){
    std::vector<Trace> requests;

    size_t previous_PBA = -1;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        if(PBA == -1){
            size_t current_write_track = get_track(write_position);

            if(isTop(current_write_track) || current_write_track == 0){
                Trace writeRequest(
                    request.time,
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
                size_t previous_write_track = get_track(previous_PBA);

                if(current_write_track != previous_write_track){
                    Trace readRequest(
                        request.time,
                        'R',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );

                    requests.push_back(readRequest);
                }

                Trace writeBottomRequest(
                    request.time,
                    'W',
                    write_position,
                    1,
                    request.device
                );
                requests.push_back(writeBottomRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;

                if(
                    i == request.size - 1
                    || current_write_track != get_track(write_position + 1)
                    || get_PBA(LBA + 1) != -1
                ){
                    Trace writeBackTopRequest(
                        request.time,
                        'W',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );
                    requests.push_back(writeBackTopRequest);
                }
            }
        }
        else{
            size_t current_update_track = get_track(PBA);

            if (isTop(current_update_track)) {
                Trace writeRequest(
                    request.time,
                    'W',
                    write_position,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);
            }
            else {
                size_t previous_update_track = get_track(previous_PBA);

                if(current_update_track != previous_update_track){
                    // * read left top track
                    if(current_update_track >= 1 && track_written[current_update_track - 1]) {
                        Trace readRequest(
                            request.time,
                            'R',
                            get_track_head(current_update_track - 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );

                        requests.push_back(readRequest);
                    }
                    // * read right top track
                    if(current_update_track < options.TRACK_NUM && track_written[current_update_track + 1]) {
                        Trace readRequest(
                            request.time,
                            'R',
                            get_track_head(current_update_track + 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );

                        requests.push_back(readRequest);
                    }
                }

                Trace writeRequest(
                    request.time,
                    'W',
                    write_position,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);

                if(
                    i == request.size - 1
                    || current_update_track != get_track(write_position + 1)
                ){
                    if(current_update_track >= 1 && track_written[current_update_track - 1]) {
                        Trace writeBackLeftTopRequest(
                            request.time,
                            'W',
                            get_track_head(current_update_track - 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );
                        requests.push_back(writeBackLeftTopRequest);
                    }
                    
                    if(current_update_track < options.TRACK_NUM && track_written[current_update_track + 1]) {
                        Trace writeBackRightTopRequest(
                            request.time,
                            'W',
                            get_track_head(current_update_track + 1),
                            options.SECTORS_PER_TOP_TRACK,
                            request.device
                        );
                        requests.push_back(writeBackRightTopRequest);
                    }
                }

            }
        }

        previous_PBA = PBA;
        write_position += 1;
    }
	
    // * output
    Trace transRequest;
    transRequest.time = request.time;
    transRequest.iotype = 'W';
    transRequest.address = request.address;
    transRequest.size = 1;
    transRequest.device = request.device;

    size_t previous_PBA = -1;

    for(size_t i = 0; i < requests.size(); ++i){
        if(requests[i].iotype == 'R'){
            output_file << requests[i];
        }
        else if(requests[i].iotype == 'W'){
            if(
                requests[i].address == (previous_PBA + 1) 
                && get_track(requests[i].address) == get_track(previous_PBA)
            ){
				transRequest.size += 1;
			}
			else{
                output_file << transRequest;

                transRequest.address = requests[i].address;
                transRequest.size = requests[i].size;
			}
			previous_PBA = requests[i].address;
        }
    }
}

void IMR_Sequential::outplace_sequential_write(const Trace &request, std::ostream &output_file){
    std::vector<Trace> requests;

    size_t previous_PBA = -1;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        if(PBA == -1){
            size_t current_write_track = get_track(write_position);

            if(isTop(current_write_track) || current_write_track == 0){
                Trace writeRequest(
                    request.time,
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
                size_t previous_write_track = get_track(previous_PBA);

                if(current_write_track != previous_write_track){
                    Trace readRequest(
                        request.time,
                        'R',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );

                    requests.push_back(readRequest);
                }

                Trace writeBottomRequest(
                    request.time,
                    'W',
                    write_position,
                    1,
                    request.device
                );
                requests.push_back(writeBottomRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;

                if(
                    i == request.size - 1
                    || current_write_track != get_track(write_position + 1)
                    || get_PBA(LBA + 1) != -1
                ){
                    Trace writeBackTopRequest(
                        request.time,
                        'W',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );
                    requests.push_back(writeBackTopRequest);
                }
            }
        }
        else{
            size_t current_write_track = get_track(write_position);

            if(isTop(current_write_track) || current_write_track == 0){
                Trace writeRequest(
                    request.time,
                    'W',
                    write_position,
                    1,
                    request.device
                );

                requests.push_back(writeRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;
            }
            else {  //write bottom track
                size_t previous_write_track = get_track(previous_PBA);

                if(current_write_track != previous_write_track){
                    Trace readRequest(
                        request.time,
                        'R',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );

                    requests.push_back(readRequest);
                }

                Trace writeBottomRequest(
                    request.time,
                    'W',
                    write_position,
                    1,
                    request.device
                );
                requests.push_back(writeBottomRequest);
                set_LBA_PBA(LBA, write_position);
                track_written[current_write_track] = true;

                if(
                    i == request.size - 1
                    || current_write_track != get_track(write_position + 1)
                ){
                    Trace writeBackTopRequest(
                        request.time,
                        'W',
                        get_track_head(current_write_track - 1),
                        options.SECTORS_PER_TOP_TRACK,
                        request.device
                    );
                    requests.push_back(writeBackTopRequest);
                }
            }
        }

        previous_PBA = PBA;
        write_position += 1;
    }

    Trace transRequest;
    transRequest.time = request.time;
    transRequest.iotype = 'W';
    transRequest.address = request.address;
    transRequest.size = 1;
    transRequest.device = request.device;

    size_t previous_PBA = -1;

    for(size_t i = 0; i < requests.size(); ++i){
        if(requests[i].iotype == 'R'){
            output_file << requests[i];
        }
        else if(requests[i].iotype == 'W'){
            if(
                requests[i].address == (previous_PBA + 1) 
                && get_track(requests[i].address) == get_track(previous_PBA)
            ){
				transRequest.size += 1;
			}
			else{
                output_file << transRequest;

                transRequest.address = requests[i].address;
                transRequest.size = requests[i].size;
			}
			previous_PBA = requests[i].address;
        }
    }
}

void IMR_Sequential::write_requests_file(const std::vector<Trace> &requests, std::ostream &out_file){
}