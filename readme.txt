Ian Gaither

Sampling Profiler

Integration:

The profiler is wrapped in an object meaning all that is required to integrate it is to link with Dbghelp.lib and include the 
source (Profiler.cpp and Profiler.h). To use it all you need to do is create an instance of the object and it will create the
log file when the object is destroyed. The profiler can be reused over the life of a program by making the scope of the object 
the same as the scope of the code you want to profile. Only a single instance can be made at once (more than one doesn't make 
sense anyway unless you're dealing with multiple threads) and it will throw an exception if one is created while another is 
active. The log file will always be sorted by the number of samples found for a specific function with the greatest coming 
first.

The properties of the profiler can be set via the constructor. You can specify the output file so you can create multiple 
profiles in a single run and not have them write over eachother. You can also set a flag to show files and line numbers for 
files that can be accessed as well as setting the minimum number of samples and the minimum cpu usage thresholds required for 
items to show up in the output log. The minimum properties are meant to help clean up a lot of the miscellaneous functions 
that appear

Implementation:

I created a sampling profiler which uses the eip found in 1ms intervals to sample code execution. These samples are stored in 
an unsorted_map with the address of the instruction being executed at the time of sampling and the number of times that address 
has been sampled. This is done to keep the per sample runtime low while avoiding having to save the address of every 
instruction sampled. This data could potentially be used to gather more information but at the moment it is being consolidated 
in another map when the log file is being written. I combine the individual instruction addresses into sums of the parent 
functions to get the total number of times each function has been sampled. From here I sort it by maximum value in a vector 
and cull small samples when writing to the log file.

Data:

The output of the profiler is a basic log file of the name specified in the code or defaulted to profile.log. The total number 
of samples as well as the properties of the profiler are displayed at the top followed by a sorted list of functions in 
decending usage order. Each function has it's overall cpu usage as well as how many samples were taken of that function 
specifically. If enabled, below each function is the file and line number the function is located at if the source can be found.