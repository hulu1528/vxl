#include <vbl/vbl_triple.h>
#include <vsl/vsl_vector_io.hxx>
#include <vbl/io/vbl_io_triple.hxx>
#include <mbl/mbl_data_collector.hxx>

typedef std::vector<vbl_triple<double,int,int> > vec_triple_dii;
MBL_DATA_COLLECTOR_INSTANTIATE( vec_triple_dii );
