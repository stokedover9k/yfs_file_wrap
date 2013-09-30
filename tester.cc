#include <iostream>
#include <sstream>
#include <cassert>

#include "yfs_file.hpp"

using namespace std;

//////////// segment_t ////////////////////

bool test_seg_split() {
  segment_t s1("abcxyz");
  assert( s1.split(3).first.text  == string("abc") );
  assert( s1.split(3).second.text == string("xyz") );

  segment_t s2(7);
  assert( s2.split(3).first.hole_size == 3 );
  assert( s2.split(3).second.hole_size == 4 );
}

bool segment_t::test_all() {
  test_seg_split();

  return true;
}

/////////// yfs_seg_list_t ////////////////

char * prep_file_content(string const & text) {
  char * ts = new char[text.size()+1];
  std::copy(text.begin(), text.end(), ts);
  std::replace(ts, ts + text.size(), '~', '\0' );
  return ts;
}

bool check_parse(string const & text) {
  char * ts = prep_file_content(text);

  ostringstream oss;
  yfs_seg_list_t seglist;
  seglist.from_file_string( ts, text.size() );
  oss << seglist.to_print();

  delete [] ts;
  cout << "[INFO] parsing \"" << text << "\" into "  << seglist << endl;
  assert( oss.str() == text );
}

bool test_yfs_file_parsing() {
  check_parse("abc~~~xyz");
  check_parse("~~~xyzxyz~~");
}

bool test_yfs_seek() {
  typename yfs_seg_list_t::file_list_t::iterator seg;
  uint offset;

  char * ts = prep_file_content("abc~~~~xyz");

  yfs_seg_list_t seglist;
  seglist.from_file_string(ts, 10);

  delete [] ts;

  assert( seglist.seek(2, seg, offset) );
  assert( seg->text == "abc" );
  assert( offset == 2 );

  assert( seglist.seek(6, seg, offset) );
  assert( seg->is_hole() );
  assert( offset == 3 );

  return true;
}

bool test_yfs_padding() {
  char * ts = prep_file_content("abc~~~xyz");

  yfs_seg_list_t seglist;
  seglist.from_file_string(ts, 9);
  seglist.pad(4);

  delete [] ts;

  assert( seglist.to_print() == "abc~~~xyz~~~~" );
  assert( seglist.size == 13 );

  return true;
}

bool test_yfs_write() {
  string filecontents("abc~~mno~~~~xyz~~~");
  string over(        "01234567890123456789");
  char * ts = prep_file_content(filecontents);

  cout << "[INFO] yfs_write over   " << filecontents << endl;

  yfs_seg_list_t seglist1;
  seglist1.from_file_string(ts, filecontents.size());

  yfs_seg_list_t seglist2;
  seglist2.from_file_string(ts, filecontents.size());

  yfs_seg_list_t seglist3;
  seglist3.from_file_string(ts, filecontents.size());

  delete [] ts;

  seglist1.write(over.substr(0,  3).c_str(), 0,  3);
  cout << "seglist 1: " << seglist1.to_print() << endl;
  assert( seglist1.to_print() == "012~~mno~~~~xyz~~~" );

  seglist2.write(over.substr(6, 10).c_str(), 6, 10);
  cout << "seglist 2: " << seglist2.to_print() << endl;
  assert( seglist2.to_print() == "abc~~m6789012345~~" );

  seglist3.write(over.substr(10, 10).c_str(), 10, 10);
  cout << "seglist 3: " << seglist3.to_print() << endl;
  assert( seglist3.to_print() == "abc~~mno~~0123456789" );

  return true;
}

bool check_read(uint offset, uint n, string const & filecontents) {
  char * ts = prep_file_content(filecontents);

  yfs_seg_list_t seglist;
  seglist.from_file_string(ts, filecontents.size());

  delete [] ts;

  char buf[n];

  int got_bytes = seglist.read(buf, offset, n);
  assert( got_bytes == min(n, uint(filecontents.size() - offset)) );
  replace(buf, buf + got_bytes, '\0', '~');
  cout << "o=" << offset << " n=" << n << " got=" << got_bytes << " res: " << string(buf, got_bytes) << endl;
  assert( string(buf, got_bytes) == filecontents.substr(offset, n) );

  return true;
}

bool test_yfs_read() {
  string filecontents("abc~~mno~~~~xyz~~~");

  cout << "[INFO] yfs_read over   " << filecontents << endl;

  check_read(0, filecontents.size(), filecontents);
  check_read(3, 2, filecontents);
  check_read(4, 10, filecontents);

  return true;
}

bool yfs_seg_list_t::test_all() {
  test_yfs_file_parsing();
  test_yfs_seek();
  test_yfs_padding();

  test_yfs_write();

  test_yfs_read();

  return true;
}

////////////////////////////////////////////

int main(int argc, char const *argv[])
{
  bool success = true;

  success = success && segment_t::test_all();
  success = success && yfs_seg_list_t::test_all();

  if( success )
    cout << "[RES] Passed all tests!" << endl;
  else
    cout << "[RES] Tests failed." << endl;
}
