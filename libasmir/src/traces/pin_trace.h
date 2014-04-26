// -*- c++ -*-

#pragma once

#include <map>
#include <iostream>
#include <fstream>
#include <memory>

#include "pin_frame.h"

#define TRACE_MAGIC 0x43525442

#define TRACE_VERSION 4

#define TRACE_ICACHE_SIZE 1024
#define TRACE_ICACHE_MASK 0x3ff

/*
 * TRACE FORMAT:
 *
 * A trace consists of a trace header (TraceHeader), followed by a
 * sequence of frames. The format of each frame is defined in
 * frame.h. Every frame begins with the same header, which contains size
 * and type information. A decoder can make use of this to determine how
 * to further parse the frame. Note that frames are variable sized.
 *
 * The trace might also contain a table of contents (TOC), which can be
 * used to quickly seek to a particular frame. The start of the table can
 * be found using the TraceHeader.toc_offset value.
 *
 * TOC FORMAT:
 *
 * The TOC consists of a header, followed by an array of unsigned 32-bit
 * integers, where each integer refers to an offset in the trace file. The
 * header contains only a single unsigned 32-bit integer, which represents
 * the length of the array that follows it. TOC.array[n] (i.e. the nth
 * element of the TOC array) is the offset into the trace file where the
 * nth keyframe can be found.
 * i.e.: TOC ==> [N] [pos 0] [pos 1] ... [pos N-1]
 *
 */

namespace pintrace { // We will use namespace to avoid collision

  typedef uint32_t addr_t;

  typedef std::map<uint64_t, uint64_t> toc_map;

   class TraceExn {
   public:
      const std::string msg;
      TraceExn(const std::string &m) : msg(m) {}
   };

   /**
    * Standard header prefixing a trace file.
    */
   struct TraceHeader {
      uint32_t magic;
      uint32_t version;
      uint64_t frame_count;
      uint64_t toc_offset;
   };

   /**
    * TraceReader: Allows playback of a trace file.
    */
   class TraceReader {

   private:

     uint64_t frm_pos;
     // toc is a map from frame number to offset
     std::auto_ptr<toc_map> toc;
     
   protected:
      std::ifstream infile;
      TraceHeader header;
      // MAX instruction byte + instruction length (1 byte)
      char icache[TRACE_ICACHE_SIZE][MAX_INSN_BYTES + 1];     
     
   public:
     TraceReader(const char *filename);
     TraceReader();
     ~TraceReader();
     
     // Open a new file
     void open(const char *filename);
     
      // Returns total number of frames in the trace.
      uint32_t count() const;

      // Returns current frame position in the trace.
      uint32_t pos() const;

      // Move frame pointer to the specified offset. Returns true iff
      // successful.
      bool seek(uint32_t offset);

      // Returns the current frame being pointed to and advances the frame
      // pointer to the next frame.
      // If noskip is false, the current frame is skipped and NULL is returned
      // instead.
      Frame *next(bool noskip = true);

      // Returns true iff the frame pointer is beyond the last frame.
      bool eof() const;

   };

   /**
    * TraceWriter: Creates a new trace file.
    */
   class TraceWriter {

   private:
      
      uint64_t frm_count;

   protected:
      
      std::ofstream outfile;

      char icache[TRACE_ICACHE_SIZE][MAX_INSN_BYTES];

   public:

      // Creates a new TraceWriter that outputs the trace to the file named
      // "filename". Will truncate the file.
      TraceWriter(const char *filename);

      // Returns the current number of frames in the trace.
      uint32_t count() const;

      // Adds a new frame to the trace.
      void add(Frame &frm);

      // Adds a vector of new frames to the trace.
      void add(std::vector<TaintFrame> &frms);
     
      // Finalizes the trace file. Will update header values if necessary,
      // and then add a TOC to the file as specified by the array
      // 'toc'. If 'toc' is NULL, then no TOC will be added, unless
      // buildTOC is true, in which case the entire trace will be
      // traversed to build the TOC structure. The file is then closed.
      // NOTE: 'toc' must be in the TOC format as specified above.
      void finalize(uint32_t *toc, bool buildTOC = false);


      // Returns the current position of the output file's put pointer,
      // i.e. the offset at which the next frame will be written into.
      // This is a helper function that can be used to determine keyframe
      // offsets, to aid in building the TOC.
      uint32_t offset()
      { return outfile.tellp(); }

   };

}; // End of namespace
