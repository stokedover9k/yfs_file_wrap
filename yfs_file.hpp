#include <list>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <cassert>
#include <ostream>
#include <istream>
#include <sstream>

#include <iostream>

struct segment_t
{
  segment_t() {}
  segment_t(uint _hole_size) : hole_size(_hole_size) {}
  segment_t(std::string _text) : text(_text) {}

  uint hole_size;
  std::string text;

  bool is_hole() const { return text.size() == 0; }
  bool is_text() const { return !is_hole(); }

  uint size() const { return is_hole() ? hole_size : text.size(); }

  std::pair<segment_t, segment_t> split(uint offset) {
    assert(offset < hole_size);
    return is_hole()
    ? std::make_pair(segment_t(offset), segment_t(size() - offset))
    : std::make_pair(segment_t(text.substr(0, offset)), segment_t(text.substr(offset)));
  }

  std::string to_print() const { return is_hole() ? std::string(size(), '~') : text; }

  void send_to_stream(std::ostream & os) const {
    if( is_hole() )  os << '[' << hole_size << ']';
    else             os << '[' << size() << ' ' << text << ']';
  }
  void read_from_stream(std::istream & is) {
    char c = ' ';
    while( c == ' ' ) is >> c;
    if( c != '[' )  throw std::runtime_error("segment_t could not be extracted from stream");
    uint sz;  is >> sz;
    is >> c;
    if( c == ' ' ) {
      text = std::string(sz, '\0');
      for (int i = 0; i < sz; ++i)
        text[i] = is.get();
    }
    else if( c == ']' ) {
      text = "";
      hole_size = sz;
    }
    else throw std::runtime_error("segment_t could not be parsed from stream");
  }

  static bool test_all();
};

void string_to_seg_list(char const * str, uint n, std::list<segment_t> & segs, uint & size) {
  size = 0;
  segs.clear();
  char const * cs = str;
  while( cs < str + n ) {
    { // get text
      std::ostringstream text;
      char const * old = cs;
      while( cs < (str + n) && (*cs != '\0') ) 
        text << *(cs++);
      if( cs != old ) {
        segs.push_back( segment_t(text.str()) );
        size += segs.back().text.length();
      }
    }
    { // get hole
      unsigned int hs = 0;
      while( cs < (str + n) && (*cs == '\0') )
        { ++hs; ++cs; }
      segs.push_back( segment_t(hs) );
      size += hs;
    }
  }
}

struct yfs_seg_list_t
{
  typedef std::list<segment_t> file_list_t;


  int read(char * buf, uint offset, uint n) {
    if( n == 0 )
      return 0;
    if( offset >= size )
      return -1;
    if( offset + n > size )
      n = size - offset;

    file_list_t::iterator seg;
    uint offset_in_seg;
    seek(offset, seg, offset_in_seg);

    read_to_buf(seg, offset_in_seg, buf, n);

    return n;
  }

  void read_to_buf(file_list_t::const_iterator seg, uint off, char * buf, uint n) {
    if( n == 0 ) return;

    uint ncopy = std::min(n, seg->size() - off);

    if( seg->is_text() )
      std::copy(seg->text.begin() + off, seg->text.begin() + off + ncopy, buf);
    else
      for (int i = 0; i < ncopy; ++i)
        buf[i] = '\0';

    read_to_buf(++seg, 0, buf + ncopy, n - ncopy);
  }

  int write(char const * buf, uint offset, uint n) {
    if( n == 0 )
      return 0;

    yfs_seg_list_t buf_list;
    buf_list.from_file_string(buf, n);

    if( offset + n > size )
      pad(offset + n - size);

    file_list_t::iterator seg1, seg2, seg12, seg22;
    uint off1, off2;

    if( offset == size ) {  seg1 = contents.end();  off1 = 0; }
    else seek(offset, seg1, off1);

    if( offset + n == size ) {  seg2 = contents.end(); off2 = 0; }
    else seek(offset + n, seg2, off2);

    if( off1 != 0 ) split(seg1, off1);
    if( off2 != 0 ) split(seg2, off2);

    // would have done it in the opposite order, if could use C++11
    contents.erase(seg1, seg2);
    contents.insert(seg2, buf_list.contents.begin(), buf_list.contents.end());

    return n;
  }

  file_list_t::iterator split(file_list_t::iterator seg, uint offset) {
    std::pair<segment_t, segment_t> p = seg->split(offset);
    *seg = p.second;
    return contents.insert(seg, p.first);
  }

  void pad(uint n) {
    // if list is empty or ends with a text segment
    // append a hole segment of length n
    if( contents.size() == 0 || contents.back().is_text() )
      contents.push_back(segment_t(n));
    // otherwise, list ends with a hole segement,
    // so widen that hole
    else
      contents.back().hole_size += n;

    size += n;
  }

  bool seek(uint offset, file_list_t::iterator & seg, uint & offset_in_seg) {
    if( offset >= size ) return false;
    seg = contents.begin();
    while( offset >= seg->size() ) {
      offset -= seg->size();
      ++seg;
    }
    offset_in_seg = offset;
    return true;
  }

  void from_file_string(char const * str, uint n) { contents.clear(); string_to_seg_list(str, n, contents, size); }

  file_list_t contents;
  uint size;

  std::string to_print() const { 
    std::ostringstream oss;
    for (file_list_t::const_iterator seg = contents.begin(); seg != contents.end(); ++seg)
      oss << seg->to_print();
    return oss.str();
  }

  static bool test_all();
};

std::ostream & operator<<(std::ostream & os, yfs_seg_list_t const & fl) {
  for (typename yfs_seg_list_t::file_list_t::const_iterator i = fl.contents.begin(); i != fl.contents.end(); ++i)
    i->send_to_stream(os);
  return os;
}
