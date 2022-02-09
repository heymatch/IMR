#include "IMR_Partition.h"

void IMR_Partition::initialize(std::ifstream &setting_file){
    // * init options
    IMR_Base::initialize(setting_file);

    // * init 
    // LBA_to_PBA.resize(options.LBA_TOTAL + 1, -1);
	// PBA_to_LBA.resize(options.LBA_TOTAL + 1, -1);
	track_written.resize(options.TOTAL_TRACKS, false);

    // * init write position
    hot_write_position = 1;
    cold_write_position = get_track_head(options.PARTITION_SIZE - 2);

    // * init first partition

	Partition newPartition;
    newPartition.head = 0;
	newPartition.size = options.PARTITION_SIZE;
    // ** hot : cold = 4 : 6
	newPartition.hot_size = (options.PARTITION_SIZE - options.BUFFER_SIZE) / 10 * 4;
	if (newPartition.hot_size % 2 == 1) {			
		newPartition.hot_size += 1;
	}
	newPartition.buffer_write_position = newPartition.buffer_head = get_track_head(newPartition.head + newPartition.hot_size);
    newPartition.buffer_PBA.resize(options.SECTORS_OF_BUFFER, -1);
    newPartition.id = 0;

	partitions.push_back(newPartition);
}

void IMR_Partition::run(std::ifstream &input_file, std::ofstream &output_file){
    read_file(input_file);
    size_t processing = 0;

    while(!order_queue.empty()){
        Request trace = order_queue.top();
        order_queue.pop();
        trace.address -= eval.shifting_address;

        if(processing % (eval.trace_requests / 100) == 0){
            std::clog << "<log> processing " << processing << "\r" << std::flush;
        }

        if(processing != 0 && processing % (eval.trace_requests / options.APPEND_PARTS) == 0){
            size_t append_size = eval.append_trace_size * options.APPEND_COLD_SIZE;
            size_t remainder = options.TOTAL_SECTORS - eval.total_sector_used;
            if(append_size > remainder){
                append_size = remainder / options.APPEND_PARTS;
            }
            std::clog << "<log> append trace " << append_size << " at processing " << processing << std::endl;

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

        processing++;
    }
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
            partitions[currentPartitionNumber].partition_reload_counts += 1;
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

void IMR_Partition::write(const Request &trace, std::ostream &output_file){
    if(trace.size > options.HOT_DATA_DEF_SIZE){
        cold_data_write(trace, output_file);
    }
    else{
        hot_data_write(trace, output_file);
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
            partitions[get_partition_position(get_track(cold_write_position))].partition_reload_counts += 1;
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
                    cold_write_position = get_track_head(get_track(cold_write_position) - 3);
                }
                else if(get_track(cold_write_position) == lastPartition.head + options.PARTITION_SIZE - 2){
                    cold_write_position = get_track_head(get_track(cold_write_position) - 2);
                }

                if(get_track(cold_write_position) < lastPartition.head + lastPartition.hot_size + options.BUFFER_SIZE + 1) {
                    lastPartition.cold_extending = true;
                    lastPartition.size += 2;
                    cold_write_position = get_track_head(lastPartition.head + lastPartition.size - 2);
                }
            }
            else{
                if(isTop(get_track(cold_write_position))){
                    if(lastPartition.size < options.MAX_PARTITION_SIZE) {	//keep expanding
                        lastPartition.size += 2;
                        cold_write_position = get_track_head(lastPartition.head + lastPartition.size - 2);
                    }
                    else{
                        Partition newPartition;
                        newPartition.head = lastPartition.head + lastPartition.size;
                        newPartition.size = options.PARTITION_SIZE;
                        newPartition.hot_size = lastPartition.hot_size / 2;
                        if (newPartition.hot_size % 2 == 1) {			
                            newPartition.hot_size += 1;
                        }
                        newPartition.buffer_write_position = newPartition.buffer_head = get_track_head(newPartition.head + newPartition.hot_size);
                        newPartition.buffer_PBA.resize(options.SECTORS_OF_BUFFER, -1);
                        newPartition.id = partitions.size();

                        partitions.push_back(newPartition);

                        hot_write_position = get_track_head(newPartition.head);
                        cold_write_position = get_track_head(newPartition.head + newPartition.size - 2);
                        cache_partition(request, partitions.size() - 1, output_file);
                    }
                }
                else{
                    cold_write_position = get_track_head(get_track(cold_write_position) - 1);
                }
            }
        }
        
        cold_write_position += 1;
        if (get_track(cold_write_position) == lastPartition.head + options.PARTITION_SIZE - 3) {
            cold_write_position = get_track_head(get_track(cold_write_position) - 3);
        }

    }

    write_requests_file(requests, output_file);
}

void IMR_Partition::hot_data_write(const Request &request, std::ostream &output_file){
    std::vector<Request> requests;

    size_t update_length = 0;

    for(size_t i = 0; i < request.size; ++i){
        size_t LBA = request.address + i;
        size_t PBA = get_PBA(LBA);

        // * write new hot data
        if(PBA == -1){
            eval.hot_write_times += 1;

            if(get_partition_position(get_track(hot_write_position)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(hot_write_position)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(hot_write_position)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(hot_write_position))].partition_reload_counts += 1;
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
            track_written.at(get_track(hot_write_position)) = true;

            hot_write_position += 1;

            // * only write on BOTTOM
            if(isTop(get_track(hot_write_position))){
                hot_write_position += options.SECTORS_PER_TOP_TRACK;
                partitions[get_partition_position(get_track(hot_write_position))].used_hot_tracks += 1;
            }

            // * if full, create new partition
            Partition &lastPartition = partitions[partitions.size() - 1];
            if (get_track(hot_write_position) > lastPartition.head + lastPartition.hot_size - 2) {	
				Partition newPartition;
                newPartition.head = lastPartition.head + lastPartition.size;
                newPartition.size = options.PARTITION_SIZE;
                newPartition.hot_size = lastPartition.hot_size * 2;
                if (newPartition.hot_size > options.PARTITION_SIZE * 0.8)
					newPartition.hot_size = options.PARTITION_SIZE * 0.8;
                if (newPartition.hot_size % 2 == 1) {			
                    newPartition.hot_size += 1;
                }
                newPartition.buffer_write_position = newPartition.buffer_head = get_track_head(newPartition.head + newPartition.hot_size);
                newPartition.buffer_PBA.resize(options.SECTORS_OF_BUFFER, -1);
                newPartition.id = partitions.size();

                partitions.push_back(newPartition);

                hot_write_position = get_track_head(newPartition.head);
                cold_write_position = get_track_head(newPartition.head + newPartition.size - 2);

                cache_partition(request, partitions.size() - 1, output_file);
			}
        }
        // * hot data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head + partitions[get_partition_position(get_track(PBA))].hot_size){
            update_length += 1;
            eval.hot_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_counts += 1;
            }
        
            try{
                size_t track = get_track(PBA);
                if(isTop(track)){
                    throw "hot data update at TOP track";
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
                exit(EXIT_FAILURE);
            }
        }
        // * buffer data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head + partitions[get_partition_position(get_track(PBA))].hot_size + options.BUFFER_SIZE){
            update_length += 1;
            eval.buffer_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_counts += 1;
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
                partitions[get_partition_position(get_track(PBA))].partition_reload_counts += 1;
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
                partitions[get_partition_position(get_track(PBA))].buffer_PBA[partitions[get_partition_position(get_track(PBA))].buffer_write_position - get_track_head(partitions[get_partition_position(get_track(PBA))].head + partitions[get_partition_position(get_track(PBA))].hot_size)] = PBA;
                partitions[get_partition_position(get_track(PBA))].buffer_write_position += 1;

                if(isTop(get_track(partitions[get_partition_position(get_track(PBA))].buffer_write_position))){
                    partitions[get_partition_position(get_track(PBA))].buffer_write_position += options.SECTORS_PER_TOP_TRACK;
                }

            }
        }
    }

    eval.insert_update_dist(update_length);

    // * output
    write_requests_file(requests, output_file);
}

void IMR_Partition::cold_data_write(const Request &request, std::ostream &output_file){
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
                partitions[get_partition_position(get_track(cold_write_position))].partition_reload_counts += 1;
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
						cold_write_position = get_track_head(get_track(cold_write_position) - 3);
					}
                    else if(get_track(cold_write_position) == lastPartition.head + options.PARTITION_SIZE - 2){
                        cold_write_position = get_track_head(get_track(cold_write_position) - 2);
                    }

                    if(get_track(cold_write_position) < lastPartition.head + lastPartition.hot_size + options.BUFFER_SIZE + 1) {
                        lastPartition.cold_extending = true;
                        lastPartition.size += 2;
                        cold_write_position = get_track_head(lastPartition.head + lastPartition.size - 2);
					}
                }
                else{
                    if(isTop(get_track(cold_write_position))){
						if(lastPartition.size < options.MAX_PARTITION_SIZE) {	//keep expanding
							lastPartition.size += 2;
							cold_write_position = get_track_head(lastPartition.head + lastPartition.size - 2);
						}
						else{
                            Partition newPartition;
                            newPartition.head = lastPartition.head + lastPartition.size;
                            newPartition.size = options.PARTITION_SIZE;
                            newPartition.hot_size = lastPartition.hot_size / 2;
                            if (newPartition.hot_size % 2 == 1) {			
                                newPartition.hot_size += 1;
                            }
                            newPartition.buffer_write_position = newPartition.buffer_head = get_track_head(newPartition.head + newPartition.hot_size);
                            newPartition.buffer_PBA.resize(options.SECTORS_OF_BUFFER, -1);
                            newPartition.id = partitions.size();

                            partitions.push_back(newPartition);

                            hot_write_position = get_track_head(newPartition.head);
                            cold_write_position = get_track_head(newPartition.head + newPartition.size - 2);
                            cache_partition(request, partitions.size() - 1, output_file);
						}
					}
					else{
						cold_write_position = get_track_head(get_track(cold_write_position) - 1);
					}
                }
            }
            
            cold_write_position += 1;
            if (get_track(cold_write_position) == lastPartition.head + options.PARTITION_SIZE - 3) {
                cold_write_position = get_track_head(get_track(cold_write_position) - 3);
            }
        }
        // * hot data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head + partitions[get_partition_position(get_track(PBA))].hot_size){
            update_length += 1;
            eval.hot_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }

            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_counts += 1;
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
            }
        }
        // * buffer data update
        else if(get_track(PBA) < partitions[get_partition_position(get_track(PBA))].head + partitions[get_partition_position(get_track(PBA))].hot_size + options.BUFFER_SIZE){
            update_length += 1;
            eval.buffer_update_times += 1;
            
            if(get_partition_position(get_track(PBA)) != latest_partition){
                cache_partition(request, get_partition_position(get_track(PBA)), output_file);
            }
            size_t lastPartitionNumber = partitions.size() - 1;
            if(get_partition_position(get_track(PBA)) != lastPartitionNumber){
                partitions[get_partition_position(get_track(PBA))].partition_reload_counts += 1;
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
                partitions[get_partition_position(get_track(PBA))].partition_reload_counts += 1;
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
                partitions[get_partition_position(get_track(PBA))].buffer_PBA[partitions[get_partition_position(get_track(PBA))].buffer_write_position - get_track_head(partitions[get_partition_position(get_track(PBA))].head + partitions[get_partition_position(get_track(PBA))].hot_size)] = PBA;
                partitions[get_partition_position(get_track(PBA))].buffer_write_position += 1;

                if(isTop(get_track(partitions[get_partition_position(get_track(PBA))].buffer_write_position))){
                    partitions[get_partition_position(get_track(PBA))].buffer_write_position += options.SECTORS_PER_TOP_TRACK;
                }

            }
        }
    }

    eval.insert_update_dist(update_length);

    // * output
    write_requests_file(requests, output_file);
}

void IMR_Partition::write_buffer(Partition &current_partition, const Request &write_request, std::ostream &output_file){
    eval.write_buffer_times += 1;

    std::vector<Request> requests;

    for (int i = 0; i < options.SECTORS_OF_BUFFER; ) {
        size_t lastPartitionNumber = partitions.size() - 1;
        if(current_partition.id != lastPartitionNumber){
            partitions[current_partition.id].partition_reload_counts += 1;
        }

		// * target is buffer's original pba
		size_t target_PBA = current_partition.buffer_PBA[i];
			
		if (target_PBA != -1) {

            // * calculating size of a sequential data

            size_t previous_PBA = current_partition.buffer_PBA[i];
            size_t seq_size;
			for (seq_size = 1; i + seq_size < options.SECTORS_OF_BUFFER; seq_size++) {
				if (
                    current_partition.buffer_PBA[i + seq_size] != previous_PBA + 1
                    || get_track(current_partition.buffer_PBA[i + seq_size]) != get_track(previous_PBA)
					|| current_partition.buffer_PBA[i + seq_size] == -1
					|| seq_size >= options.SECTORS_PER_TOP_TRACK
                ){
					break;
				}
				previous_PBA = current_partition.buffer_PBA[i + seq_size];
			}

			// * read from buffer
            {
                Request readRequest(
                    write_request.timestamp,
                    'R',
                    get_track_head(current_partition.head + current_partition.hot_size) + i,
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
                    get_track_head(get_track(target_PBA) - 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );
                requests.push_back(readRequest);
			}
            if (track_written[get_track(target_PBA) + 1]) {
                Request readRequest(
                    write_request.timestamp,
                    'R',
                    get_track_head(get_track(target_PBA) + 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );
                requests.push_back(readRequest);
			}

			// * read target segment
            size_t segments;
            size_t track_head_addr = get_track_head(get_track(target_PBA));
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
                    get_track(target_PBA) + 1 >= current_partition.head + current_partition.size - 1
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
                    get_track_head(get_track(target_PBA) - 1),
                    options.SECTORS_PER_TOP_TRACK,
                    write_request.device
                );

                requests.push_back(writeRequest);
			}
			if (track_written[get_track(target_PBA) + 1]) {
                Request writeRequest(
                    write_request.timestamp,
                    'W',
                    get_track_head(get_track(target_PBA) + 1),
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
                if(get_track(target_PBA) + 1 < current_partition.head + current_partition.size - 1){
                    write_addr = get_track_head(get_track(target_PBA) + 1);
                }
                else{
                    write_addr = get_track_head(get_track(target_PBA) - 1);
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
				for (int j = 0; j < write_size; j++) {
					// * move segments from top to bottom
                    size_t write_PBA = write_addr + j;
                    size_t write_LBA = get_LBA(write_PBA);
                    size_t buffer_PBA = buffer_addr + j;

                    set_LBA_to_PBA(write_LBA, buffer_PBA);
                    // set_PBA_to_LBA(buffer_PBA, write_LBA);
				}
				// * update mapping of sectors from buffer
				for (int j = 0; j < seq_size; j++) {
                    size_t buffer_PBA = get_track_head(current_partition.head + current_partition.hot_size) + i + j;
                    size_t buffer_LBA = get_LBA(buffer_PBA);
                    size_t new_PBA = target_PBA + j - buffer_addr + write_addr;

                    set_LBA_to_PBA(buffer_LBA, new_PBA);
                    // set_PBA_to_LBA(new_PBA, buffer_LBA);
				}
            }

			// if (get_track(target_PBA) + 1 < current_partition.head + current_partition.size - 1) { 
            //     size_t write_addr = get_track_head(get_track(target_PBA) + 1);
            //     size_t write_size = segments * options.SEGMENT_SIZE;

            //     Request writeRequest(
            //         write_request.timestamp,
            //         'W',
            //         write_addr,
            //         write_size,
            //         write_request.device
            //     );

            //     requests.push_back(writeRequest);

			// 	// * update mapping
			// 	for (int j = 0; j < write_size; j++) {
			// 		// * move segments from top to bottom
            //         size_t write_PBA = write_addr + j;
            //         size_t write_LBA = get_LBA(write_PBA);
            //         size_t buffer_PBA = buffer_addr + j;

            //         set_LBA_PBA(write_LBA, buffer_PBA);
			// 	}
			// 	// * update mapping of sectors from buffer
			// 	for (int j = 0; j < seq_size; j++) {
            //         size_t buffer_PBA = get_track_head(current_partition.head + current_partition.hot_size) + i + j;
            //         size_t buffer_LBA = get_LBA(buffer_PBA);
            //         size_t new_PBA = target_PBA + j - buffer_addr + write_addr;

            //         set_LBA_PBA(buffer_LBA, new_PBA);
			// 	}
			// }
			// else {
            //     size_t write_addr = get_track_head(get_track(target_PBA) - 1);
            //     size_t write_size = segments * options.SEGMENT_SIZE;

            //     Request writeRequest(
            //         write_request.timestamp,
            //         'W',
            //         write_addr,
            //         write_size,
            //         write_request.device
            //     );

            //     requests.push_back(writeRequest);

			// 	// * update mapping
			// 	for (int j = 0; j < write_size; j++) {
			// 		// * move segments from top to bottom
            //         size_t write_PBA = write_addr + j;
            //         size_t write_LBA = get_LBA(write_PBA);
            //         size_t buffer_PBA = buffer_addr + j;

            //         set_LBA_PBA(write_LBA, buffer_PBA);
			// 	}
			// 	// * update mapping of sectors from buffer
			// 	for (int j = 0; j < seq_size; j++) {
            //         size_t buffer_PBA = get_track_head(current_partition.head + current_partition.hot_size) + i + j;
            //         size_t buffer_LBA = get_LBA(buffer_PBA);
            //         size_t new_PBA = target_PBA + j - buffer_addr + write_addr;

            //         set_LBA_PBA(buffer_LBA, new_PBA);
			// 	}
			// }

			i += seq_size;
		}
		else {
			i += 1;
		}
	}

	current_partition.buffer_write_position = get_track_head(current_partition.head + current_partition.hot_size);
    std::fill(current_partition.buffer_PBA.begin(), current_partition.buffer_PBA.end(), -1);
    eval.write_buffer_requests += requests.size();

    write_requests_file(requests, output_file);
}

void IMR_Partition::cache_partition(const Request &request, const size_t &partition_number, std::ostream &output_file){
    eval.cache_check_times += 1;
    latest_partition = partition_number;
	for(int i = 0; i < mapping_cache.size(); i++) {
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
            get_track_head(partitions[mapping_cache.front()].head + options.PARTITION_SIZE - 3),
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
        get_track_head(partitions[partition_number].head + options.PARTITION_SIZE - 3),
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

    evaluation_stream << "Last Hot Write Position: "    << hot_write_position           << "\n";
    evaluation_stream << "Last Cold Write Position: "   << cold_write_position          << "\n";
    

    evaluation_stream << "Hot Write Sectors: "          << eval.hot_write_times         << "\n";
    evaluation_stream << "Hot Update Sectors: "         << eval.hot_update_times        << "\n";

    evaluation_stream << "Cold Write Sectors: "         << eval.cold_write_sectors      << "\n";
    evaluation_stream << "Cold Update Sectors: "        << eval.cold_update_times       << "\n";

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
        sum_partition_size += partitions[i].size;
        sum_partition_hot_size += partitions[i].hot_size;
    }
    avg_partition_size = (double) sum_partition_size / partitions.size();
    avg_partition_hot_size = (double) sum_partition_hot_size / partitions.size();

    evaluation_stream << "Total Partitions: " << partitions.size() << "\n";
    evaluation_stream << "Average Partition Size: " << avg_partition_size << "\n";
    evaluation_stream << "Average Partition Hot Size: " << avg_partition_hot_size << "\n";
    evaluation_stream << "Average Partition Cold Size: " << 0 << "\n";
    evaluation_stream << "Average Partition Buffer Size: " << 0 << "\n";
    evaluation_stream << "Sum Partition Size: " << sum_partition_size << "\n";

    distribution_stream << "Partitions Status:\n";
    distribution_stream << "Partition,Used_Hot_Tracks,Allocated_Hot_Size,Used_Cold_Tracks,Cache_Load_Counts,Partition_Reload_Sector_Counts\n";
    for(size_t i = 0; i < partitions.size(); ++i){
        distribution_stream << 
            std::setw(6) << i                                       << "," <<
            std::setw(6) << partitions[i].used_hot_tracks           << "," <<
            std::setw(6) << partitions[i].hot_size                  << "," <<
            std::setw(6) << partitions[i].used_cold_tracks          << "," <<
            std::setw(6) << partitions[i].cache_load_counts         << "," <<
            std::setw(6) << partitions[i].partition_reload_counts   << "\n";
    }
}