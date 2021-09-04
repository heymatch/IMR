#include <bits/stdc++.h>
using namespace std;

#include "IMR_Partition.h"
#include "IMR_Sequential.h"
#include "IMR_Crosstrack.h"

int main(int argc, char **argv){
    if(argc != 4){
        cerr << "<error> expected 4 arguments" << endl;
        cerr << "1st argument, IMR mode: 4modes, partition" << endl;
        cerr << "2nd argument, input file " << endl;
        cerr << "3rd argument, output file" << endl;
        cerr << "4th argument, setting file" << endl;
    }

    ifstream input_file(argv[2]);
    ofstream output_file(argv[3]);
    ifstream setting_file(argv[4]);

    if(input_file.fail() || output_file.fail() || setting_file.fail()){
        cerr << "<error> open file error" << endl;
    }

    if(string(argv[1]) == "partition"){
        using namespace IMR_Partition;

        initialize(setting_file);
        run(input_file, output_file);
        evaluation();
    }
    else if(string(argv[1]) == "seqnential"){
        using namespace IMR_Sequential;
        
        initialize(setting_file);
        run(input_file, output_file);
        evaluation();
    }
    else if(string(argv[1]) == "crosstrack"){
        
    }
    else{
        cerr << "<error> wrong 1st argument" << endl;
    }

    

    return 0;
}