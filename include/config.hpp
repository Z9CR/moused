#pragma once
// we will store these props in a file in the future
// thus, just temporally store them in .c

// use char* to avoid huuuuuuge STL
extern const char* cfg_path;

extern const char* macro_path;

// when smooth moving, 
// prog will slice the path to dest into pieces,
// and the value below is how long will stay in per pieces
// unit: ms
extern const int smoothmv_frametime;