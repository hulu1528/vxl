// This is vil/io/vil_io_memory_image_of.txx
#ifndef vil_io_memory_image_of_txx_
#define vil_io_memory_image_of_txx_

#include <vil/io/vil_io_memory_image_of.h>
#include <vil/io/vil_io_memory_image.h>
#include <vil/vil_memory_image_of.h>
#include <vsl/vsl_clipon_binary_loader.txx>
#include <vil/io/vil_io_image_impl.h>
//:
// \file
// \brief Perform serialised binary IO for vil_memory_image_of<T>
// \author Ian Scott (Manchester)
// \date 27-Mar-2001

//: Binary write to stream.
template <class T>
void vsl_b_write(vsl_b_ostream &os, const vil_memory_image_of<T> & p)
{
  // write version number
  const short io_version_no = 1;
  vsl_b_write(os, io_version_no);
  vsl_b_write(os, (const vil_memory_image &) p);
}

//========================================================================
//: Binary load from stream.
template <class T>
void vsl_b_read(vsl_b_istream &is, vil_memory_image_of<T> & p)
{
  if (!is) return;

  short v;
  vsl_b_read(is, v);
  switch(v)
  {
  case 1:
    {
      vsl_b_read(is, (vil_memory_image &) p);
      p.recache_from_impl();
    }
    break;

  default:
    vcl_cerr << "I/O ERROR: vsl_b_read(vsl_b_istream&, vil_memory_image_of<T>&) \n";
    vcl_cerr << "           Unknown version number "<< v << "\n";
    is.is().clear(vcl_ios::badbit); // Set an unrecoverable IO error on stream
    return;
  }
}


//========================================================================
//: Output a human readable summary to the stream
template <class T>
void vsl_print_summary(vcl_ostream& os,const vil_memory_image_of<T> & p)
{
    vsl_print_summary(os, p.impl());
}


#define VIL_IO_MEMORY_IMAGE_OF_INSTANTIATE(T) \
template void vsl_print_summary(vcl_ostream&, vil_memory_image_of<T >const&); \
template void vsl_b_read(vsl_b_istream &, vil_memory_image_of<T >&); \
template void vsl_b_write(vsl_b_ostream &, const vil_memory_image_of<T >&)

#endif // vil_io_memory_image_of_txx_
