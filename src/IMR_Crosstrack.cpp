#include "IMR_Crosstrack.h"

void IMR_Crosstrack::initialize(std::ifstream &setting_file){
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
    }

    // * init 
	track_written.resize(options.TRACK_NUM, false);

    // * init write position
    write_position = 1;
}

void IMR_Crosstrack::run(std::ifstream &input_file, std::ofstream &output_file){
    read_file(input_file);
    size_t processing = 0;

    while(!order_queue.empty()){
        Request trace = order_queue.top();
        order_queue.pop();

        if(processing++ % 1000000 == 0)
            std::clog << "<log> processing " << processing << std::endl;

        // * read request
        if(trace.iotype == 'R' || trace.iotype == '1'){
            trace.iotype = 'R';
            read(trace, output_file);
        }
        // * write request
        else if(trace.iotype == 'W' || trace.iotype == '0'){
            trace.iotype = 'W';
            write(trace, output_file);
        }
    }
}

void IMR_Crosstrack::write(const Request &trace, std::ostream &output_file){
    if(options.UPDATE_METHOD == Update_Method::IN_PLACE){
        inplace_crosstrack_write(trace, output_file);
    }
    else if(options.UPDATE_METHOD == Update_Method::OUT_PLACE){
        outplace_crosstrack_write(trace, output_file);
    }
}

void IMR_Crosstrack::inplace_crosstrack_write(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    size_t previous_PBA = -1;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        if(PBA == -1){
            size_t current_write_track = get_track(write_position);
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
            
            if (current_write_track != get_track(write_position + 1)) {
				write_position = get_track_head((current_write_track) + 2);
                
				if (!isTop(current_write_track) && current_write_track >= options.TRACK_NUM) {
                    // * move to first TOP track
					write_position = get_track_head(1);
				}
			}
			else
				write_position += 1;
        }
        else{
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
                size_t previous_update_track = get_track(previous_PBA);

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
        }

        previous_PBA = PBA;
    }
	
    // * output
    write_requests_file(requests, output_file);
}

void IMR_Crosstrack::outplace_crosstrack_write(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        if(PBA == -1){
            size_t current_write_track = get_track(write_position);
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
            
            if (current_write_track != get_track(write_position + 1)) {
				write_position = get_track_head((current_write_track) + 2);
                
				if (!isTop(current_write_track) && current_write_track >= options.TRACK_NUM) {
                    // * move to first TOP track
					write_position = get_track_head(1);
				}
			}
			else
				write_position += 1;
        }
        // * PBA exists, update
        else{
            size_t current_update_track = get_track(PBA);

            if(
                isTop(current_update_track) 
                || (current_update_track == 0 && !track_written[current_update_track + 1])
                || (!track_written[current_update_track - 1] && !track_written[current_update_track + 1])
            ){
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
            }
            else{
                size_t current_write_track = get_track(write_position);
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
                
                if (current_write_track != get_track(write_position + 1)) {
                    write_position = get_track_head((current_write_track) + 2);
                    
                    if (!isTop(current_write_track) && current_write_track >= options.TRACK_NUM) {
                        // * move to first TOP track
                        write_position = get_track_head(1);
                    }
                }
                else
                    write_position += 1;
            }
        }
    }

    // * output
    write_requests_file(requests, output_file);
}

void IMR_Crosstrack::evaluation(std::ofstream &evaluation_file){
    evaluation_file << std::fixed << std::setprecision(2);
    
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
}