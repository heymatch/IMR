#include "IMR_Partition.h"

IMR_Partition::Partition::Partition(const IMR_Partition &IMR, const size_t head, const IMR_Partition::Partition::HOT_ZONE hot_zone){
    const Partition &lastPartition = IMR.partitions.back();
    head_track = head;
    if(head_track % 2 == 1) head_track += 1;

    if(hot_zone == IMR_Partition::Partition::HOT_ZONE::INIT){
        // * init H:C = 4:6
        alloc_hot_track_size = (IMR.options.BASE_PARTITION_TRACK_SIZE - IMR.options.BASE_BUFFER_TRACK_SIZE) / 10 * 4;
        if(alloc_hot_track_size % 2 == 1) alloc_hot_track_size += 1;
    }
    else if(hot_zone == IMR_Partition::Partition::HOT_ZONE::APPROPRIATE){
        alloc_hot_track_size = lastPartition.alloc_hot_track_size;
    }
    else if(hot_zone == IMR_Partition::Partition::HOT_ZONE::TOO_LARGE){
        alloc_hot_track_size = lastPartition.alloc_hot_track_size / 2;
        // if(alloc_hot_track_size < IMR.options.BASE_PARTITION_TRACK_SIZE * 0.1) alloc_hot_track_size = IMR.options.BASE_PARTITION_TRACK_SIZE * 0.1;
        if(alloc_hot_track_size % 2 == 1) alloc_hot_track_size += 1;
    }
    else if(hot_zone == IMR_Partition::Partition::HOT_ZONE::TOO_SMALL){
        alloc_hot_track_size = lastPartition.alloc_hot_track_size * 2;
        if(alloc_hot_track_size > IMR.options.BASE_PARTITION_TRACK_SIZE * 0.8) alloc_hot_track_size = IMR.options.BASE_PARTITION_TRACK_SIZE * 0.8;
        if(alloc_hot_track_size % 2 == 1) alloc_hot_track_size += 1;
    }

    // alloc_track_size = IMR.options.BASE_PARTITION_TRACK_SIZE;
    alloc_track_size = IMR.options.BASE_PARTITION_TRACK_SIZE;

    buffer_write_position = buffer_head_sector = IMR.get_track_head_sector(head_track + alloc_hot_track_size + buffer_track_size - 1);
    buffer_tail_sector = IMR.get_track_head_sector(head_track + alloc_hot_track_size + 2);
    buffer_PBA.resize(IMR.options.TOTAL_BUFFER_SECTOR_SIZE, -1);
    id = IMR.partitions.size();
}

void IMR_Partition::initialize(std::ifstream &setting_file){
    // * init options, settings
    IMR_Base::initialize(setting_file);

    // * init 
	track_written.resize(options.TOTAL_TRACKS, false);

    // * init first partition
	Partition newPartition(*this, 0, IMR_Partition::Partition::HOT_ZONE::INIT);
    partitions.push_back(newPartition);

    // * init write position
    hot_write_position = 0;
    cold_write_position = get_track_head_sector(newPartition.head_track + newPartition.alloc_hot_track_size + newPartition.buffer_track_size + 1);
}

void IMR_Partition::run(std::ifstream &input_file, std::ofstream &output_file){
    read_file(input_file);

    while(!order_queue.empty()){
        Request trace = order_queue.top();
        order_queue.pop();
        trace.address -= eval.shifting_address;

        if(eval.processing % (eval.trace_total_requests / 100) == 0){
            // std::clog << "<debug> hot_write_position " << hot_write_position << std::endl;
            // std::clog << "<debug> cold_write_position " << cold_write_position << std::endl;
            // size_t total_track_used = 0;
            // for(size_t i = 0; i < track_written.size(); ++i){
            //     if(track_written[i]) ++total_track_used;
            // }
            // std::clog << "<debug> Total Track Used: "           << total_track_used             << "\n";
            std::clog << "<log> processing " << eval.processing << "\r" << std::flush;
        }

        if(eval.processing != 0 && eval.processing % (eval.trace_total_requests / options.APPEND_PARTS) == 0){
            size_t append_size = eval.append_trace_size * options.APPEND_COLD_SIZE;
            size_t remainder = options.TOTAL_SECTORS - eval.total_sector_used;
            if(append_size > remainder){
                append_size = remainder / options.APPEND_PARTS;
            }
            std::clog << "<log> append trace " << append_size << " at processing " << eval.processing << std::endl;

            for(size_t append = append_size; append > 0 && append > (1 << 20); append -= 1 << 20){
                Request append_trace(
                    trace.timestamp,
                    'W',
                    eval.max_LBA + 1,
                    1 << 20,
                    trace.device
                );
                write_append(append_trace, output_file);
                eval.total_sector_used += 1 << 20;
            }

            eval.append_count += 1;
        }

        // * read request
        if(trace.iotype == 'R' || trace.iotype == '1'){
            trace.iotype = '1';
            IMR_Partition::read(trace, output_file);
        }
        // * write request
        else if(trace.iotype == 'W' || trace.iotype == '0'){
            trace.iotype = '0';
            write(trace, output_file);
        }

        eval.processing++;
    }
}

void IMR_Partition::request(Request &request){

}

void IMR_Partition::read(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    for (int i = 0; i < request.size; i++) {
        size_t readPBA = get_PBA(request.address + i);
		if(readPBA == -1){
            write(request, output_file);
            readPBA = get_PBA(request.address + i);
        }

        size_t currentPartitionNumber = get_partition_position(get_track(readPBA));

        if(currentPartitionNumber != latest_partition){
            cache_partition(request, currentPartitionNumber, output_file);
        }

        size_t lastPartitionNumber = partitions.size() - 1;
        if(currentPartitionNumber != lastPartitionNumber){
            partitions[currentPartitionNumber].partition_reload_sector_count += 1;
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

void IMR_Partition::write(const Request &request, std::ostream &output_file){
    if(request.size > options.HOT_DATA_DEF_SIZE){
        eval.cold_write_request_count += 1;
        cold_data_write(request, output_file);
    }
    else{
        eval.hot_write_request_count += 1;
        hot_data_write(request, output_file);
    }
}

void IMR_Partition::write_append(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address;

        eval.cold_write_sectors += 1;

        if(get_partition_position(get_track(cold_write_position)) != latest_partition){
            cache_partition(request, get_partition_position(get_track(cold_write_position)), output_file);
        }

        size_t lastPartitionNumber = partitions.size() - 1;
        if(get_partition_position(get_track(cold_write_position)) != lastPartitionNumber){
            partitions[get_partition_position(get_track(cold_write_position))].partition_reload_sector_count += 1;
        }

        Request writeRequest(
            request.timestamp,
            'W',
            cold_write_position,
            1,
            request.device
        );
        requests.push_back(writeRequest);
        set_LBA_to_PBA(LBA, cold_write_position);
        // set_PBA_to_LBA(cold_write_position, LBA);
        track_written.at(get_track(cold_write_position)) = true;

        Partition &lastPartition = partitions[partitions.size() - 1];
        if(get_track(cold_write_position) != get_track(cold_write_position + 1)){
            
            lastPartition.used_cold_tracks += 1;

            if(!lastPartition.cold_extending){
                if(isTop(get_track(cold_write_position))){
                    cold_write_position = get_track_head_sector(get_track(cold_write_position) - 3);
                }
                else if(get_track(cold_write_position) == lastPartition.head_track + options.BASE_PARTITION_TRACK_SIZE - 2){
                    cold_write_position = get_track_head_sector(get_track(cold_write_position) - 2);
                }

                if(get_track(cold_write_position) < lastPartition.head_track + lastPartition.alloc_hot_track_size + options.BASE_BUFFER_TRACK_SIZE + 1) {
                    lastPartition.cold_extending = true;
                    lastPartition.alloc_track_size += 2;
                    cold_write_position = get_track_head_sector(lastPartition.head_track + lastPartition.alloc_track_size - 2);
                }
            }
            else{
                if(isTop(get_track(cold_write_position))){
                    if(lastPartition.alloc_track_size < options.MAX_PARTITION_SIZE) {	//keep expanding
                        lastPartition.alloc_track_size += 2;
                        cold_write_position = get_track_head_sector(lastPartition.head_track + lastPartition.alloc_track_size - 2);
                    }
                    else{
                        Partition newPartition;
                        newPartition.head_track = lastPartition.head_track + lastPartition.alloc_track_size;
                        newPartition.alloc_track_size = options.BASE_PARTITION_TRACK_SIZE;
                        newPartition.alloc_hot_track_size = lastPartition.alloc_hot_track_size / 2;
                        if (newPartition.alloc_hot_track_size % 2 == 1) {			
                            newPartition.alloc_hot_track_size += 1;
                        }
                        newPartition.buffer_write_position = newPartition.buffer_head_sector = get_track_head_sector(newPartition.head_track + newPartition.alloc_hot_track_size + newPartition.buffer_track_size - 1);
                        newPartition.buffer_tail_sector = get_track_head_sector(newPartition.head_track + newPartition.alloc_hot_track_size + 3) - 1;
                        newPartition.buffer_PBA.resize(options.TOTAL_BUFFER_SECTOR_SIZE, -1);
                        newPartition.id = partitions.size();

                        partitions.push_back(newPartition);

                        hot_write_position = get_track_head_sector(newPartition.head_track);
                        cold_write_position = get_track_head_sector(newPartition.head_track + newPartition.alloc_hot_track_size + options.BASE_BUFFER_TRACK_SIZE);
                        cache_partition(request, partitions.size() - 1, output_file);
                    }
                }
                else{
                    cold_write_position = get_track_head_sector(get_track(cold_write_position) - 1);
                }
            }
        }
        
        cold_write_position += 1;
        if (get_track(cold_write_position) == lastPartition.head_track + options.BASE_PARTITION_TRACK_SIZE - 3) {
            cold_write_position = get_track_head_sector(get_track(cold_write_position) - 3);
        }

    }

    write_requests_file(requests, output_file);
}

void IMR_Partition::hot_data_write(const Request &request, std::ostream &output_file){
    // std::clog << "<debug> begin of hot write" << std::endl;

    std::vector<Request> requests;

    size_t update_length = 0;

    for(size_t req_offset = 0; req_offset < request.size; ++req_offset){
        size_t LBA = request.address + req_offset;
        size_t PBA = get_PBA(LBA);

        // * write new hot data
        if(PBA == -1){
            eval.hot_write_sector_count += 1;

            if(get_partition_position(get_track(hot_write_position)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(hot_write_position)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(hot_write_position)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(hot_write_position))].partition_reload_sector_count += 1;
            }

            Request writeRequest(
                request.timestamp,
                'W',
                hot_write_position,
                1,
                request.device
            );
            requests.push_back(writeRequest);
            set_LBA_to_PBA(LBA, hot_write_position);
            // set_PBA_to_LBA(hot_write_position, LBA);

            size_t write_track = get_track(hot_write_position);
            try{
                if(isTop(write_track)){
                    throw "<error> hot data write at TOP track";
                }
            }
            catch(const char* e){
                std::cerr << "<exception>" << e << std::endl;
                exit(EXIT_FAILURE);
            }
            track_written.at(get_track(hot_write_position)) = true;

            hot_write_position += 1;

            // * only write on BOTTOM
            if(isTop(get_track(hot_write_position))){
                hot_write_position += options.SECTORS_PER_TOP_TRACK;
                partitions[get_partition_position(get_track(hot_write_position))].used_hot_tracks += 1;
            }

            // * if full, create new partition
            // ** extend hot tracks

            Partition &lastPartition = partitions.back();
            if(get_track(hot_write_position) > lastPartition.head_track + lastPartition.alloc_hot_track_size - 2){
                Partition newPartition;
                size_t used_tracks = lastPartition.alloc_hot_track_size + lastPartition.buffer_track_size + 1 + lastPartition.cold_track_size;
                if(lastPartition.cold_extending){
                    newPartition = Partition(*this, lastPartition.head_track + used_tracks + 2, IMR_Partition::Partition::HOT_ZONE::APPROPRIATE);
                }
                else{
                    newPartition = Partition(*this, lastPartition.head_track + used_tracks + 2, IMR_Partition::Partition::HOT_ZONE::TOO_SMALL);
                }
                hot_write_position = get_track_head_sector(newPartition.head_track);
                cold_write_position = get_track_head_sector(newPartition.head_track + newPartition.alloc_hot_track_size + newPartition.buffer_track_size + 1);
                
                partitions.push_back(newPartition);
                cache_partition(request, partitions.size() - 1, output_file);

                // std::clog << "<debug> end allocate hot partition" << std::endl;
                // std::clog << "<debug> newPartition.head_track: " << newPartition.head_track << std::endl;
                // std::clog << "<debug> get_track(newPartition.buffer_tail_sector): " << get_track(newPartition.buffer_tail_sector) << std::endl;
			}
        }
        // * hot data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head_track + partitions[get_partition_position(get_track(PBA))].alloc_hot_track_size){
            update_length += 1;
            eval.hot_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_sector_count += 1;
            }
        
            try{
                size_t track = get_track(PBA);
                if(isTop(track)){
                    std::cerr << "<error note> track: " << track << std::endl;
                    throw "<exception> hot data update at TOP track";
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
            }
            catch(const char *e){
                std::cerr << e << std::endl;
                exit(EXIT_FAILURE);
            }

        }
        // * buffer data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head_track + partitions[get_partition_position(get_track(PBA))].alloc_hot_track_size + options.BASE_BUFFER_TRACK_SIZE){
            update_length += 1;
            eval.buffer_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_sector_count += 1;
            }
            
            try{
                size_t track = get_track(PBA);
                if(isTop(track)){
                    throw "buffer data update at TOP track";
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
            }
            catch(const char *e){
                std::cerr << e << std::endl;
                exit(EXIT_FAILURE);
            }

        }
        // * cold data update
        else{
            update_length += 1;
            eval.cold_update_times += 1;

            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }
            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_sector_count += 1;
            }

            size_t track = get_track(PBA);
            // * if TOP, direct update
            if(isTop(track)){
                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
            }
            // * if BOTTOM, out-place update to buffer
            else{
                Partition &current_partition = partitions[get_partition_position(get_track(PBA))];

                // * if buffer full, write buffer back to original tracks 
                if(current_partition.isBufferFull(options)){
                    write_buffer(current_partition, request, output_file);
                }

                size_t track = get_track(current_partition.buffer_write_position);
                if(isTop(track)){
                    throw "buffer data write at TOP track";
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    current_partition.buffer_write_position,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
                track_written[get_track(current_partition.buffer_write_position)] = true;

                set_LBA_to_PBA(request.address, current_partition.buffer_write_position);
                // set_PBA_to_LBA(partitions[get_partition_position(get_track(PBA))].buffer_write_position, request.address);
                current_partition.buffer_PBA[current_partition.buffer_write_count++] = PBA;
                current_partition.buffer_write_position += 1;

                if(isTop(get_track(current_partition.buffer_write_position))){
                    current_partition.buffer_write_position = get_track_head_sector(get_track(current_partition.buffer_write_position) - 3);
                }

            }
        }
    }

    eval.insert_update_dist(update_length);

    // * output
    write_requests_file(requests, output_file);

    // std::clog << "<debug> end of hot write" << std::endl;
}

void IMR_Partition::cold_data_write(const Request &request, std::ostream &output_file){
    // std::clog << "<debug> begin of cold write" << std::endl;

    std::vector<Request> requests;

    size_t update_length = 0;
    
    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        // TODO current partition

        // * write new cold data
        if(PBA == -1){
            eval.cold_write_sectors += 1;

            if(get_partition_position(get_track(cold_write_position)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(cold_write_position)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(cold_write_position)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(cold_write_position))].partition_reload_sector_count += 1;
            }

            Request writeRequest(
                request.timestamp,
                'W',
                cold_write_position,
                1,
                request.device
            );
            requests.push_back(writeRequest);
            set_LBA_to_PBA(LBA, cold_write_position);
            // set_PBA_to_LBA(cold_write_position, LBA);
            track_written.at(get_track(cold_write_position)) = true;

            Partition &lastPartition = partitions.back();
            if(get_track(cold_write_position) != get_track(cold_write_position + 1)){
                
                lastPartition.used_cold_tracks += 1;
                lastPartition.cold_track_size += 1;

                if(lastPartition.cold_extending){
                    if(isTop(get_track(cold_write_position))){
                        if(lastPartition.alloc_track_size < options.MAX_PARTITION_SIZE){
                            lastPartition.alloc_track_size += 2;
                            cold_write_position = get_track_head_sector(lastPartition.head_track + lastPartition.alloc_track_size - 2);
                        }
                        else{
                            size_t used_tracks = lastPartition.alloc_hot_track_size + lastPartition.buffer_track_size + 1 + lastPartition.cold_track_size;
                            Partition newPartition(*this, lastPartition.head_track + used_tracks + 2, IMR_Partition::Partition::HOT_ZONE::TOO_LARGE);
                            partitions.push_back(newPartition);

                            hot_write_position = get_track_head_sector(newPartition.head_track);
                            cold_write_position = get_track_head_sector(newPartition.head_track + newPartition.alloc_hot_track_size + newPartition.buffer_track_size + 1);
                            cache_partition(request, partitions.size() - 1, output_file);
                        }
                    }
                    else{
                        cold_write_position = get_track_head_sector(get_track(cold_write_position) - 1);
                    }
                }
                else{
                    if(isTop(get_track(cold_write_position))){
                        cold_write_position = get_track_head_sector(get_track(cold_write_position) + 3);
                    }
                    else{
                        cold_write_position = get_track_head_sector(get_track(cold_write_position) - 1);
                    }

                    // * extend cold tracks
                    if(get_track(cold_write_position) >= lastPartition.head_track + lastPartition.alloc_track_size){
                        lastPartition.cold_extending = true;
                        lastPartition.alloc_track_size += 2;
                        // cold_write_position = get_track_head_sector(lastPartition.head_track + lastPartition.alloc_track_size - 2);
					}
                }

                // if(!lastPartition.cold_extending){
                //     if(isTop(get_track(cold_write_position))){
				// 		cold_write_position = get_track_head_sector(get_track(cold_write_position) - 3);
				// 	}
                //     else if(get_track(cold_write_position) == lastPartition.head_track + options.BASE_PARTITION_TRACK_SIZE - 2){
                //         cold_write_position = get_track_head_sector(get_track(cold_write_position) - 2);
                //     }

                //     if(get_track(cold_write_position) < lastPartition.head_track + lastPartition.hot_track_size + options.BASE_BUFFER_TRACK_SIZE + 1) {
                //         lastPartition.cold_extending = true;
                //         lastPartition.alloc_track_size += 2;
                //         cold_write_position = get_track_head_sector(lastPartition.head_track + lastPartition.alloc_track_size - 2);
				// 	}
                // }
                // else{
                //     if(isTop(get_track(cold_write_position))){
				// 		if(lastPartition.alloc_track_size < options.MAX_PARTITION_SIZE) {	//keep expanding
				// 			lastPartition.alloc_track_size += 2;
				// 			cold_write_position = get_track_head_sector(lastPartition.head_track + lastPartition.alloc_track_size - 2);
				// 		}
				// 		else{
                //             Partition newPartition;
                //             newPartition.head_track = lastPartition.head_track + lastPartition.alloc_track_size;
                //             newPartition.alloc_track_size = options.BASE_PARTITION_TRACK_SIZE;
                //             newPartition.hot_track_size = lastPartition.hot_track_size / 2;
                //             if (newPartition.hot_track_size % 2 == 1) {			
                //                 newPartition.hot_track_size += 1;
                //             }
                //             newPartition.buffer_write_position = newPartition.buffer_head_sector = get_track_head_sector(newPartition.head_track + newPartition.hot_track_size);
                //             newPartition.buffer_PBA.resize(options.TOTAL_BUFFER_SECTOR_SIZE, -1);
                //             newPartition.id = partitions.size();

                //             partitions.push_back(newPartition);

                //             hot_write_position = get_track_head_sector(newPartition.head_track);
                //             cold_write_position = get_track_head_sector(newPartition.head_track + newPartition.hot_track_size + options.BASE_BUFFER_TRACK_SIZE);
                //             cache_partition(request, partitions.size() - 1, output_file);
				// 		}
				// 	}
				// 	else{
				// 		cold_write_position = get_track_head_sector(get_track(cold_write_position) - 1);
				// 	}
                // }
            }
            
            cold_write_position += 1;
            // if (get_track(cold_write_position) == lastPartition.head_track + options.BASE_PARTITION_TRACK_SIZE - 3) {
            //     cold_write_position = get_track_head_sector(get_track(cold_write_position) - 3);
            // }
        }
        // * hot data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head_track + partitions[get_partition_position(get_track(PBA))].alloc_hot_track_size){
            update_length += 1;
            eval.hot_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_sector_count += 1;
            }

            try{
                size_t track = get_track(PBA);
                if(isTop(track)){
                    throw "<exception> hot data update at TOP track";
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
            }
            catch(const char *e){
                std::cerr << e << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        // * buffer data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head_track + partitions[get_partition_position(get_track(PBA))].alloc_hot_track_size + partitions[get_partition_position(get_track(PBA))].buffer_track_size){
            update_length += 1;
            eval.buffer_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }
            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_sector_count += 1;
            }
            
            try{
                size_t track = get_track(PBA);
                if(isTop(track)){
                    throw "buffer data update at TOP track";
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
            }
            catch(const char *e){
                std::cerr << "<exception>" << e << std::endl;
            }
        }
        // * cold data update
        else{
            update_length += 1;
            eval.cold_update_times += 1;

            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }
            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_sector_count += 1;
            }

            size_t track = get_track(PBA);
            // * if TOP, direct update
            if(isTop(track)){
                Request writeRequest(
                    request.timestamp,
                    'W',
                    PBA,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
            }
            // * if BOTTOM, out-place update to buffer
            else{
                // * if buffer full, write buffer back to original tracks 
                if(partitions[get_partition_position(get_track(PBA))].isBufferFull(options)){
                    write_buffer(partitions[get_partition_position(get_track(PBA))], request, output_file);
                }
                // std::clog << "<log> partitions[get_partition_position(get_track(PBA))].buffer_write_position: " << partitions[get_partition_position(get_track(PBA))].buffer_write_position << std::endl;
                // std::clog << "<log> options.SECTORS_OF_BUFFER: " << options.SECTORS_OF_BUFFER << std::endl;

                size_t track = get_track(partitions[get_partition_position(get_track(PBA))].buffer_write_position);
                if(isTop(track)){
                    throw "buffer data write at TOP track";
                }

                Request writeRequest(
                    request.timestamp,
                    'W',
                    partitions[get_partition_position(get_track(PBA))].buffer_write_position,
                    1,
                    request.device
                );
                requests.push_back(writeRequest);
                track_written[get_track(partitions[get_partition_position(get_track(PBA))].buffer_write_position)] = true;

                set_LBA_to_PBA(request.address, partitions[get_partition_position(get_track(PBA))].buffer_write_position);
                // set_PBA_to_LBA(partitions[get_partition_position(get_track(PBA))].buffer_write_position, request.address);
                partitions[get_partition_position(get_track(PBA))].buffer_PBA[partitions[get_partition_position(get_track(PBA))].buffer_write_count++] = PBA;
                partitions[get_partition_position(get_track(PBA))].buffer_write_position += 1;

                if(isTop(get_track(partitions[get_partition_position(get_track(PBA))].buffer_write_position))){
                    partitions[get_partition_position(get_track(PBA))].buffer_write_position = get_track_head_sector(get_track(partitions[get_partition_position(get_track(PBA))].buffer_write_position) - 3);
                }

            }
        }
    }

    eval.insert_update_dist(update_length);

    // * output
    write_requests_file(requests, output_file);

    // std::clog << "<debug> end of cold write" << std::endl;
}

void IMR_Partition::write_buffer(Partition &current_partition, const Request &write_request, std::ostream &output_file){
    std::clog << "<debug> begin of write buffer" << std::endl;
    eval.write_buffer_times += 1;

    std::vector<Request> requests;

    for (size_t buffer_offset = 0; buffer_offset < options.TOTAL_BUFFER_SECTOR_SIZE; ) {
        size_t lastPartitionNumber = partitions.size() - 1;
        if(current_partition.id != lastPartitionNumber){
            partitions[current_partition.id].partition_reload_sector_count += 1;
        }

		// * target is buffer's original pba
		size_t target_PBA = current_partition.buffer_PBA[buffer_offset];
			
		if (target_PBA != -1) {

            // * calculating size of a sequential data

            size_t previous_PBA = current_partition.buffer_PBA[buffer_offset];
            size_t seq_size;
			for (seq_size = 1; buffer_offset + seq_size < options.TOTAL_BUFFER_SECTOR_SIZE; seq_size++) {
				if (
                    current_partition.buffer_PBA[buffer_offset + seq_size] != previous_PBA + 1
                    || get_track(current_partition.buffer_PBA[buffer_offset + seq_size]) != get_track(previous_PBA)
					|| current_partition.buffer_PBA[buffer_offset + seq_size] == -1
					|| seq_size >= options.SECTORS_PER_TOP_TRACK
                ){
					break;
				}
				previous_PBA = current_partition.buffer_PBA[buffer_offset + seq_size];
			}

			// * read from buffer
            {
                Request readRequest(
                    write_request.timestamp,
                    'R',
                    get_track_head_sector(current_partition.head_track + current_partition.alloc_hot_track_size) + buffer_offset,
                    seq_size,
                    write_request.device
                );
                requests.push_back(readRequest);
            }

			// * read top tracks
			if (track_written[get_track(target_PBA) - 1]) {
                Request readRequest(
                    write_request.timestamp,
                    'R',
                    get_track_head_sector(get_track(target_PBA) - 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );
                requests.push_back(readRequest);
			}
            if (track_written[get_track(target_PBA) + 1]) {
                Request readRequest(
                    write_request.timestamp,
                    'R',
                    get_track_head_sector(get_track(target_PBA) + 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );
                requests.push_back(readRequest);
			}

			// * read target segment
            size_t segments;
            size_t track_head_addr = get_track_head_sector(get_track(target_PBA));
            size_t buffer_addr = track_head_addr + ((target_PBA - track_head_addr) / options.SEGMENT_SIZE) * options.SEGMENT_SIZE;

            {
                size_t segment_range = target_PBA + seq_size - buffer_addr;

                segments = segment_range / options.SEGMENT_SIZE;
                if(segment_range % options.SEGMENT_SIZE != 0 || segments == 0){
                    segments += 1;
                }

                Request readRequest(
                    write_request.timestamp,
                    'R',
                    buffer_addr,
                    segments * options.SEGMENT_SIZE,
                    write_request.device
                );
                requests.push_back(readRequest);
            }
		
			// * write top track's segments to bottom tracks
			if (
                track_written[get_track(target_PBA) + 1]
                || (
                    // * last cold tracks can't be written
                    get_track(target_PBA) + 1 >= current_partition.head_track + current_partition.alloc_track_size - 1
                    && track_written[get_track(target_PBA) - 1]
                )
            ) 
            { 
                Request writeRequest(
                    write_request.timestamp,
                    'W',
                    buffer_addr,
                    segments * options.SEGMENT_SIZE,
                    write_request.device
                );

                requests.push_back(writeRequest);
			}

			// * write back top tracks
			if (track_written[get_track(target_PBA) - 1]) {
                Request writeRequest(
                    write_request.timestamp,
                    'W',
                    get_track_head_sector(get_track(target_PBA) - 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );

                requests.push_back(writeRequest);
			}
			if (track_written[get_track(target_PBA) + 1]) {
                Request writeRequest(
                    write_request.timestamp,
                    'W',
                    get_track_head_sector(get_track(target_PBA) + 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );

                requests.push_back(writeRequest);
			}

			// * write segments from bottom tracks into top tracks
            {
                size_t write_addr;
                size_t write_size = segments * options.SEGMENT_SIZE;

                // * last cold tracks can't be written
                if(get_track(target_PBA) + 1 < current_partition.head_track + current_partition.alloc_track_size - 1){
                    write_addr = get_track_head_sector(get_track(target_PBA) + 1);
                }
                else{
                    write_addr = get_track_head_sector(get_track(target_PBA) - 1);
                }

                Request writeRequest(
                    write_request.timestamp,
                    'W',
                    write_addr,
                    write_size,
                    write_request.device
                );

                requests.push_back(writeRequest);

				// * update mapping
				for (size_t j = 0; j < write_size; j++) {
					// * move segments from top to bottom
                    size_t write_PBA = write_addr + j;
                    size_t write_LBA = get_LBA(write_PBA);
                    size_t buffer_PBA = buffer_addr + j;

                    set_LBA_to_PBA(write_LBA, buffer_PBA);
                    // set_PBA_to_LBA(buffer_PBA, write_LBA);
				}
				// * update mapping of sectors from buffer
				for (size_t j = 0; j < seq_size; j++) {
                    size_t buffer_PBA = get_track_head_sector(current_partition.head_track + current_partition.alloc_hot_track_size) + buffer_offset + j;
                    size_t buffer_LBA = get_LBA(buffer_PBA);
                    size_t new_PBA = target_PBA + j - buffer_addr + write_addr;

                    set_LBA_to_PBA(buffer_LBA, new_PBA);
                    // set_PBA_to_LBA(new_PBA, buffer_LBA);
				}
            }

			buffer_offset += seq_size;
		}
		else {
			buffer_offset += 1;
		}
	}

	current_partition.buffer_write_position = current_partition.buffer_head_sector;
    current_partition.buffer_write_count = 0;
    std::fill(current_partition.buffer_PBA.begin(), current_partition.buffer_PBA.end(), -1);
    eval.write_buffer_requests += requests.size();

    write_requests_file(requests, output_file);

    std::clog << "<debug> end of write buffer" << std::endl;
}

void IMR_Partition::cache_partition(const Request &request, const size_t &partition_number, std::ostream &output_file){
    eval.cache_check_times += 1;
    latest_partition = partition_number;
	for(size_t i = 0; i < mapping_cache.size(); i++) {
		if (mapping_cache[i] == partition_number) {
			return;
		}
	}

    std::vector<Request> requests;
    if (mapping_cache.size() >= options.MAPPING_CACHE_SIZE) {
        // * write map back into old partition's map track
        Request writeRequest(
            request.timestamp,
            'W',
            get_track_head_sector(partitions[mapping_cache.front()].head_track + partitions[mapping_cache.front()].alloc_hot_track_size + partitions[mapping_cache.front()].buffer_track_size + 1),
            options.SECTORS_PER_TOP_TRACK,
            request.device
        );
        requests.push_back(writeRequest);
        eval.cache_write_times += 1;
        mapping_cache.pop_front();
    }

    // * read new partition's map track
    Request writeRequest(
        request.timestamp,
        'R',
        get_track_head_sector(partitions[mapping_cache.front()].head_track + partitions[mapping_cache.front()].alloc_hot_track_size + partitions[mapping_cache.front()].buffer_track_size + 1),
        options.SECTORS_PER_TOP_TRACK,
        request.device
    );
    requests.push_back(writeRequest);

    eval.cache_load_times += 1;
    partitions[partition_number].cache_load_counts += 1;
    mapping_cache.push_back(partition_number);
	
    write_requests_file(requests, output_file);
}

void IMR_Partition::evaluation(std::string &evaluation_file){
    IMR_Base::evaluation(evaluation_file);

    evaluation_stream << "=== Partition Evaluation ==="                                 << "\n";

    evaluation_stream << "Last Hot Write Position: "    << hot_write_position           << "\n";
    evaluation_stream << "Last Cold Write Position: "   << cold_write_position          << "\n";
    
    evaluation_stream << "Hot Write Request Count: "    << eval.hot_write_request_count  << "\n";
    evaluation_stream << "Hot Write Request Ratio: "    << ((double)eval.hot_write_request_count) / eval.trace_total_requests  << "\n";
    evaluation_stream << "Hot Write Sector Count: "     << eval.hot_write_sector_count  << "\n";
    evaluation_stream << "Hot Update Sector Count: "    << eval.hot_update_times        << "\n";

    evaluation_stream << "Cold Write Request Count: "   << eval.cold_write_request_count  << "\n";
    evaluation_stream << "Cold Write Request Ratio: "   << ((double)eval.cold_write_request_count) / eval.trace_total_requests  << "\n";
    evaluation_stream << "Cold Write Sector Count: "    << eval.cold_write_sectors      << "\n";
    evaluation_stream << "Cold Update Sector Count: "   << eval.cold_update_times       << "\n";

    evaluation_stream << "Cache Check Requests: "       << eval.cache_check_times       << "\n";
    evaluation_stream << "Cache Load Requests: "        << eval.cache_load_times        << "\n";
    evaluation_stream << "Cache Write Requests: "       << eval.cache_write_times       << "\n";

    evaluation_stream << "Buffer Update Sectors: "      << eval.buffer_update_times     << "\n";
    evaluation_stream << "Write Buffer Sectors: "       << eval.write_buffer_times      << "\n";
    evaluation_stream << "Write Buffer Requests: "      << eval.write_buffer_requests   << "\n";

    double avg_partition_size = 0.0;
    size_t sum_partition_size = 0;
    double avg_partition_hot_size = 0.0;
    size_t sum_partition_hot_size = 0;
    double avg_partition_cold_size = 0.0;
    size_t sum_partition_cold_size = 0;
    double avg_partition_buffer_size = 0.0;
    size_t sum_partition_buffer_size = 0;

    for(size_t i = 0; i < partitions.size(); ++i){
        // sum_partition_size += partitions[i].alloc_track_size;
        sum_partition_size += partitions[i].alloc_hot_track_size + partitions[i].cold_track_size + partitions[i].buffer_track_size + 1;
        sum_partition_hot_size += partitions[i].alloc_hot_track_size;
        sum_partition_cold_size += partitions[i].cold_track_size;
        sum_partition_buffer_size += partitions[i].buffer_track_size;
    }
    avg_partition_size = (double) sum_partition_size / partitions.size();
    avg_partition_hot_size = (double) sum_partition_hot_size / partitions.size();
    avg_partition_cold_size = (double) sum_partition_cold_size / partitions.size();
    avg_partition_buffer_size = (double) sum_partition_buffer_size / partitions.size();

    evaluation_stream << "Total Partitions Count: "                 << partitions.size()            << "\n";
    evaluation_stream << "Average Partition Allocated Size: "       << avg_partition_size           << "\n";
    evaluation_stream << "Average Partition Hot Track Size: "       << avg_partition_hot_size       << "\n";
    evaluation_stream << "Average Partition Cold Track Size: "      << avg_partition_cold_size       << "\n";
    evaluation_stream << "Average Partition Buffer Track Size: "    << avg_partition_buffer_size       << "\n";
    evaluation_stream << "Sum Partition Allocated Size: "           << sum_partition_size                   << "\n";
    evaluation_stream << "Sum Partition Hot Track Size: "           << sum_partition_hot_size               << "\n";
    evaluation_stream << "Sum Partition Cold Track Size: "          << sum_partition_cold_size              << "\n";
    evaluation_stream << "Sum Partition Buffer Track Size: "        << sum_partition_buffer_size            << "\n";

    distribution_stream << "Partitions Status:\n";
    distribution_stream << "Partition_ID,Track_Head,Used_Hot_Tracks,Allocated_Hot_Size,Used_Cold_Tracks,Cache_Load_Counts,Partition_Reload_Sector_Counts\n";
    for(size_t i = 0; i < partitions.size(); ++i){
        distribution_stream << 
            std::setw(6) << i                                       << "," <<
            std::setw(6) << partitions[i].head_track           << "," <<
            std::setw(6) << partitions[i].used_hot_tracks           << "," <<
            std::setw(6) << partitions[i].alloc_hot_track_size                  << "," <<
            std::setw(6) << partitions[i].cold_track_size          << "," <<
            std::setw(6) << partitions[i].cache_load_counts         << "," <<
            std::setw(6) << partitions[i].partition_reload_sector_count   << "\n";
    }
}