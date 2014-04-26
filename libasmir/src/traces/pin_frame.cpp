#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "pin_frame.h"
//#include "pin_taint.h"

using namespace std;
using namespace pintrace;

int pintrace::source = 1;

#define READ(__in, val) __in.read((char *) &val, (streamsize) sizeof(val))
#define WRITE(__out, val)  __out.write((char *) (&val), (streamsize) sizeof(val))

// Returns the number of bits in a VT type
uint32_t bitsOfType(uint32_t t) {
  switch(t) {
      case VT_REG8:
      case VT_MEM8:
        return 8;
        break;
        
      case VT_REG16:
      case VT_MEM16:
        return 16;
        break;

      case VT_REG32:
      case VT_MEM32:
        return 32;
        break;

      case VT_REG64:
      case VT_MEM64:
        return 64;
        break;

      case VT_REG128:
      case VT_MEM128:
        return 128;
        break;

      default:
        cerr << "Invalid type: " << t << endl;
        assert(false);
        
  }
  
  return 0;
}

// Return the number of bytes needed to store type t
uint32_t bytesOfType(uint32_t t) {
  uint32_t b = bitsOfType(t);
  assert((b % 8) == 0);
  return (b / 8);
}

// Returns true if rm is a valid RegMem_t
bool pintrace::valid_regmem_type(RegMem_t rm) {
    return (pintrace::NONE <= rm.type && rm.type <= pintrace::MEM);
}


Frame *Frame::unserialize(istream &in, bool noskip)
{

   uint8_t packed_type;
   FrameType type;
   uint16_t size;
   Frame *f = NULL;

   //   cerr << "unserialize at " << in.tellg();

   READ(in, packed_type);
   READ(in, size);   

   type = (FrameType) packed_type;

   //   cerr << " type " << type << " size " << size << endl;

   if (!noskip) {
     //      printf("Skipping frame, %d bytes.\n", size - 3);
      in.ignore(size - 3);
      return NULL;
   }

   switch(type) {
       case FRM_STD:
         f = new StdFrame;
         break;
       case FRM_STD2:
         f = new StdFrame2;
         break;
       case FRM_KEY:
         f = new KeyFrame;
         break;
       case FRM_KEY_GENERAL:
         f = new KeyFrameGeneral;
         break;
       case FRM_LOADMOD:
         f = new LoadModuleFrame;
         break;
       case FRM_SYSCALL:
         f = new SyscallFrame;
         break;
       case FRM_TAINT:
         f = new TaintFrame;
         break;
       case FRM_EXCEPT:
         f = new ExceptionFrame;
         break;
       case FRM_NONE:
       default:
      // TODO: Error handling here.
      printf("ERROR: Unknown frame type %d!\n", type);
      assert(false);
      break;
   }

   f->unserializePart(in);
   return f;

}


// Writes a serialized version of the frame into buffer 'out'. The
// buffer must be of at least this.size bytes long.
// Returns a pointer to the character just after the last character
// written to the buffer.
ostream &Frame::serialize(ostream &out, uint16_t sz)
{

   sz += 3;                     // 3 == sizeof((char) type) + sizeof(sz)

   // Compress type value into a single byte.
   unsigned char packed_type = (char) type;

   WRITE(out, packed_type);
   WRITE(out, sz);
   return out;

}

void StdFrame::clearCache()
{
   memset((void *) &cachemask, 0, MAX_CACHEMASK_BTYES);
}

void StdFrame2::clearCache()
{
   memset((void *) &cachemask, 0, MAX_CACHEMASK_BTYES);
}

ostream &StdFrame::serialize(ostream &out, uint16_t sz)
{


   // Note: ((x-1) >> 3) + 1 === ceil(x / 8.0)
   uint32_t masklen = ((values_count - 1) >> 3) + 1;

   // 9 = sizeof(addr) + sizeof(tid) + sizeof(lengths)
   sz += 9
      + (insn_length * sizeof(char))
      + masklen
      + (values_count * sizeof(uint32_t));

   ostream &out2 = Frame::serialize(out, sz);

   WRITE(out2, addr);
   WRITE(out2, tid);

   // The two length values (insn_length and values_count) are packed into a
   // single byte like so:
   // |7654|3210|
   //    |    \-- values_count
   //    \------- insn_length

   uint8_t lengths = (values_count & 0xf) | (insn_length << 4);

   WRITE(out2, lengths);

   out2.write((const char *) &rawbytes, insn_length * sizeof(char));
   out2.write((const char *) &cachemask, masklen);
   out2.write((const char *) &values, values_count * sizeof(uint32_t));
   out2.write((const char *) &types, values_count * sizeof(uint32_t));
   out2.write((const char *) &usages, values_count * sizeof(uint32_t));
   out2.write((const char *) &locs, values_count * sizeof(uint32_t));
   out2.write((const char *) &taint, values_count * sizeof(uint32_t));

   return out2;

}

istream &StdFrame::unserializePart(istream &in)
{

   READ(in, addr);
   READ(in, tid);
 
   uint8_t lengths;
   READ(in, lengths);

   insn_length = lengths >> 4;
   values_count = lengths & 0xf;

   // Be a bit paranoid.
   if (insn_length > MAX_INSN_BYTES) insn_length = MAX_INSN_BYTES;

   in.read((char *) &rawbytes, insn_length * sizeof(char));

   // Note: ((x-1) >> 3) + 1 === ceil(x / 8.0)
   in.read((char *) &cachemask, ((values_count - 1) >> 3) + 1);

   in.read((char *) &values, values_count * sizeof(uint32_t));
   in.read((char *) &types, values_count * sizeof(uint32_t));
   in.read((char *) &usages, values_count * sizeof(uint32_t));
   in.read((char *) &locs, values_count * sizeof(uint32_t));
   in.read((char *) &taint, values_count * sizeof(uint32_t));

   return in;

}

#ifdef GET_OPERANDS
conc_map_vec * StdFrame::getOperands()
{
  assert(false);
}
#endif

ostream &StdFrame2::serialize(ostream &out, uint16_t sz)
{
   // Note: ((x-1) >> 3) + 1 === ceil(x / 8.0)
   uint32_t masklen = ((values_count - 1) >> 3) + 1;

   // 10 = sizeof(addr) + sizeof(tid) + sizeof(values_count) +
   // sizeof(insn_length) + sizeof(types, usages, locs, taint)
   sz += 10
     + (insn_length * sizeof(char))
     + masklen
     + (values_count * sizeof(uint32_t) * 4);

   for (int i = 0; i < values_count; i++) {
     sz += bytesOfType(types[i]);
   }

   ostream &out2 = Frame::serialize(out, sz);

   WRITE(out2, addr);
   WRITE(out2, tid);

   // The two length values (insn_length and values_count) are packed into a
   // single byte like so:
   // |7654|3210|
   //    |    \-- values_count
   //    \------- insn_length

   //uint8_t lengths = (values_count & 0xf) | (insn_length << 4);

   WRITE(out2, values_count);
   WRITE(out2, insn_length);

   out2.write((const char *) &rawbytes, insn_length * sizeof(char));
   out2.write((const char *) &cachemask, masklen);
   out2.write((const char *) &types, values_count * sizeof(uint32_t));
   out2.write((const char *) &usages, values_count * sizeof(uint32_t));
   out2.write((const char *) &locs, values_count * sizeof(uint32_t));
   out2.write((const char *) &taint, values_count * sizeof(uint32_t));

   for (int i = 0; i < values_count; i++) {
     out2.write((const char *) &(values[i]), bytesOfType(types[i]));
   }

   return out2;

}

istream &StdFrame2::unserializePart(istream &in)
{

   READ(in, addr);
   READ(in, tid);

   READ(in, values_count);
   READ(in, insn_length);

   // Be a bit paranoid.
   if (insn_length > MAX_INSN_BYTES) insn_length = MAX_INSN_BYTES;

   in.read((char *) &rawbytes, insn_length * sizeof(char));

   // Note: ((x-1) >> 3) + 1 === ceil(x / 8.0)
   in.read((char *) &cachemask, ((values_count - 1) >> 3) + 1);

   in.read((char *) &types, values_count * sizeof(uint32_t));
   in.read((char *) &usages, values_count * sizeof(uint32_t));
   in.read((char *) &locs, values_count * sizeof(uint32_t));
   in.read((char *) &taint, values_count * sizeof(uint32_t));

   // Read in the values
   assert (values_count <= MAX_VALUES_COUNT);
   for (int i = 0; i < values_count; i++) {
     in.read((char *) &(values[i]), bytesOfType(types[i]));
   }
   
   return in;

}

#ifdef GET_OPERANDS
conc_map_vec * StdFrame2::getOperands()
{
  conc_map_vec * concrete_pairs = new vector<conc_map *>();
  int i, type, t, usage; bool mem;
  size_t bytes;
  string name;
  const_val_t index, value;
  conc_map * map;
  for ( i = 0 ; i < values_count ; i ++ )
    {
      big_val_t tval;
      switch (types[i])
	{
	case VT_REG128:
	case VT_REG64:
	case VT_REG32: 
	case VT_REG16: 
	case VT_REG8: 
	  bytes = bytesOfType(types[i]);
	  name = pin_register_name(locs[i]);
	  mem = false;
	  value = 0;
          for (int j = 0; j < ((bytes + (sizeof(const_val_t)-1))/sizeof(const_val_t)); j++) {
            /* Insert each chunk, while preserving the byte order */
            memcpy(&value, &(values[i].byte[j*sizeof(const_val_t)]), sizeof(const_val_t));
            //cerr << hex << "Pushing back name " << name << " value: " << value << " bytes: " << bytes << endl;
            tval.push_back(value);
          }

	  usage = usages[i] ;
	  t = taint[i] ;
	  map = new ConcPair(name,mem,get_type(types[i]),index,
			     tval,usage,t);
	  concrete_pairs->push_back(map);
	  break;
	case VT_MEM128:
	case VT_MEM64:
	case VT_MEM32: 
	case VT_MEM16: 
	case VT_MEM8: 
	  bytes = bytesOfType(types[i]);
	  name = "mem";
	  mem = true;
	  index = locs[i] ;

          for (int j = 0; j < ((bytes + (sizeof(const_val_t)-1))/sizeof(const_val_t)); j++) {
            /* Insert each chunk, while preserving the byte order */
            memcpy(&value, &(values[i].byte[j*sizeof(const_val_t)]), sizeof(const_val_t));
            //cerr << hex << "Pushing back mem " << value << endl;
            tval.push_back(value);
          }

          usage = usages[i] ;
	  t = taint[i] ;
	  map = new ConcPair(name,mem,get_type(types[i]),index,
			     tval,usage,t);
	  concrete_pairs->push_back(map);
	  break ;
	default : 
	  //cerr << "WARNING: Ignoring operand of type: " << types[i] << endl ; 
	  //              assert(0) ;
	  break;
	}
    }
  return concrete_pairs;
}
#endif

ostream &KeyFrame::serialize(ostream &out, uint16_t sz)
{

   ostream &out2 = Frame::serialize(out, sz + 56);
   
   WRITE(out2, pos); // 8

   WRITE(out2, eax); // 4*9
   WRITE(out2, ebx);
   WRITE(out2, ecx);
   WRITE(out2, edx);
   WRITE(out2, esi);
   WRITE(out2, edi);
   WRITE(out2, esp);
   WRITE(out2, ebp);
   WRITE(out2, eflags);

   WRITE(out2, cs); // 2*6
   WRITE(out2, ds);
   WRITE(out2, ss);
   WRITE(out2, es);
   WRITE(out2, fs);
   WRITE(out2, gs);

   return out2;

}

ostream &KeyFrameGeneral::serialize(ostream &out, uint16_t sz)
{

  sz += 8 + 4 + (4*numRegs) + (4*numRegs) + 4 + (4*numMems) + (1*numMems);
  for (unsigned int i = 0; i < numRegs; i++) {
    sz += bytesOfType(regTypes[i]);
  }

   ostream &out2 = Frame::serialize(out, sz);

   WRITE(out2, pos); // 8

   WRITE(out2, numRegs); // 4
   out2.write((const char *) regIds, numRegs * sizeof(regIds[0])); // 4*numRegs
   out2.write((const char *) regTypes, numRegs * sizeof(regTypes[0])); // 4*numRegs
   for (unsigned int i = 0; i < numRegs; i++) {
     out2.write((const char *) regValues, bytesOfType(regTypes[i])); // variable
   }
   WRITE(out2, numMems); // 4
   out2.write((const char *) memAddrs, numMems * sizeof(memAddrs[0])); // 4*numMems
   out2.write((const char *) memValues, numMems * sizeof(memValues[0])); // 1*numMems

   return out2;
}


istream &KeyFrame::unserializePart(istream &in)
{

   READ(in, pos);

   READ(in, eax);
   READ(in, ebx);
   READ(in, ecx);
   READ(in, edx);
   READ(in, esi);
   READ(in, edi);
   READ(in, esp);
   READ(in, ebp);
   READ(in, eflags);

   READ(in, cs);
   READ(in, ds);
   READ(in, ss);
   READ(in, es);
   READ(in, fs);
   READ(in, gs);

   return in;

}

istream &KeyFrameGeneral::unserializePart(istream &in)
{

   READ(in, pos);
   READ(in, numRegs);

   // Read register information
   regIds = new uint32_t[numRegs];
   assert(regIds);
   regTypes = new uint32_t[numRegs];
   assert(regTypes);
   regValues = new PIN_REGISTER[numRegs];
   assert(regValues);
   in.read((char *) regIds, numRegs * sizeof(regIds[0]));
   in.read((char *) regTypes, numRegs * sizeof(regTypes[0]));
   for (unsigned int i = 0; i < numRegs; i++) {
     in.read((char *) regValues, bytesOfType(regTypes[i]));
   }

   // Read memory information
   READ(in, numMems);

   memAddrs = new uint32_t[numMems];
   memValues = new uint8_t[numMems];
   in.read((char *) memAddrs, numMems * sizeof(memAddrs[0]));
   in.read((char *) memValues, numMems * sizeof(memValues[0]));
   
   return in;

}

#ifdef GET_OPERANDS
conc_map_vec * KeyFrameGeneral::getOperands()
{
  conc_map_vec * concrete_pairs = new vector<conc_map *>();
  int i, type, t, usage; bool mem;
  size_t bytes;
  string name;
  const_val_t index, value;
  conc_map * map;
  for ( i = 0 ; i < numRegs ; i ++ )
    {
      big_val_t tval;
      switch (regTypes[i])
	{
            case VT_REG128:
            case VT_REG64:
            case VT_REG32:
            case VT_REG16:
            case VT_REG8:
              bytes = bytesOfType(regTypes[i]);
              name = pin_register_name(regIds[i]);
              mem = false;
              value = 0;
              for (int j = 0; j < ((bytes + (sizeof(const_val_t)-1))/sizeof(const_val_t)); j++) {
                /* Insert each chunk, while preserving the byte order */
                memcpy(&value, &(regValues[i].byte[j*sizeof(const_val_t)]), sizeof(const_val_t));
                //cerr << hex << "Pushing back name " << name << " value: " << value << " bytes: " << bytes << endl;
                tval.push_back(value);
              }

              usage = 0;
              t = 0;
              map = new ConcPair(name,mem,get_type(regTypes[i]),index,
                                 tval,usage,t);
              concrete_pairs->push_back(map);
              break;

            default:
              assert(false);
              break;
        }
    }

  for (i = 0; i < numMems; i++) {
    bytes = 1;
    name = "mem";
    mem = true;
    index = memAddrs[i] ;

    value = memValues[i];
    //cerr << hex << "Pushing back mem " << value << endl;
    big_val_t tval;
    tval.push_back(value);

    usage = 0 ;
    t = 0;
    map = new ConcPair(name,mem,get_type(VT_MEM8),index,
                       tval,usage,t);
    concrete_pairs->push_back(map);
  }

  return concrete_pairs;
}
#endif

void KeyFrame::setAll(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx,
                      uint32_t esi, uint32_t edi, uint32_t esp, uint32_t ebp,
                      uint32_t eflags,
                      uint16_t cs, uint16_t ds, uint16_t ss,
                      uint16_t es, uint16_t fs, uint16_t gs)
{

   this->eax = eax;
   this->ebx = ebx;
   this->ecx = ecx;
   this->edx = edx;
   this->esi = esi;
   this->edi = edi;
   this->esp = esp;
   this->ebp = ebp;
   this->eflags = eflags;

   this->cs = cs;
   this->ds = ds;
   this->ss = ss;
   this->es = es;
   this->fs = fs;
   this->gs = gs;

}


ostream &LoadModuleFrame::serialize(ostream &out, uint16_t sz)
{

   ostream &out2 = Frame::serialize(out, sz + 80);

   WRITE(out2, low_addr);
   WRITE(out2, high_addr);
   WRITE(out2, start_addr);
   WRITE(out2, load_offset);
   WRITE(out2, name);

   return out2;

}

istream &LoadModuleFrame::unserializePart(istream &in)
{

   READ(in, low_addr);
   READ(in, high_addr);
   READ(in, start_addr);
   READ(in, load_offset);
   READ(in, name);

   return in;

}


ostream &SyscallFrame::serialize(ostream &out, uint16_t sz)
{
   ostream &out2 = Frame::serialize(out, sz + 12 + 4*MAX_SYSCALL_ARGS);

   WRITE(out2, addr);
   WRITE(out2, tid);
   WRITE(out2, callno);

   for (int i = 0; i < MAX_SYSCALL_ARGS; i++) {
      WRITE(out2, args[i]);
   }

   return out2;

}

istream &SyscallFrame::unserializePart(istream &in)
{

   READ(in, addr);
   READ(in, tid);
   READ(in, callno);

   for (int i = 0; i < MAX_SYSCALL_ARGS; i++) {
      READ(in, args[i]);
   }

   return in;

}

#ifdef GET_OPERANDS
conc_map_vec * SyscallFrame::getOperands()
{
  conc_map_vec * concrete_pairs = new vector<conc_map *>();
  int type, usage, taint; bool mem;
  string name;
  const_val_t index, value;
  conc_map * map;
  uint32_t bytes = callno;
  uint32_t i;
  big_val_t tval;
  //  tval.push_back(0);
  for ( i = 0 ; i < callno ; i ++ )
    {
      name = "mem";
      mem = true;
      index = args[1] + i ;
      //      value = 0;
      usage = 0;
      taint = source++ ;
      map = new ConcPair(name,mem,get_type(VT_MEM8),index,
			 tval,usage,taint);
      concrete_pairs->push_back(map);
    }
  return concrete_pairs;
}
#endif

ostream &TaintFrame::serialize(ostream &out, uint16_t sz)
{
   
   ostream &out2 = Frame::serialize(out, sz + 12);

   WRITE(out2, id);
   WRITE(out2, length);
   WRITE(out2, addr);

   return out2;

}

istream &TaintFrame::unserializePart(istream &in)
{

   READ(in, id);
   READ(in, length);
   READ(in, addr);

   return in;

}

#ifdef GET_OPERANDS
conc_map_vec * TaintFrame::getOperands()
{
	conc_map_vec * concrete_pairs = new vector<conc_map *>();
	int type, usage, taint; bool mem;
	string name;
	const_val_t index, value;
	conc_map * map;
	uint32_t i;
	usage = 0;
        big_val_t tval;
        // tval.push_back(0);
        for ( i = 0 ; i < length ; i ++ )
	{
		name = "mem";
		mem = true;
		index = addr + i ;
                //		value = 0;
		taint = id;
		map = new ConcPair(name,mem,get_type(VT_MEM8),index,
				   tval,usage,taint);
		concrete_pairs->push_back(map);
	}
	return concrete_pairs;
}
#endif

ostream &ExceptionFrame::serialize(ostream &out, uint16_t sz)
{
   
   ostream &out2 = Frame::serialize(out, sz + 16);

   WRITE(out2, exception);
   WRITE(out2, tid);
   WRITE(out2, from_addr);
   WRITE(out2, to_addr);

   return out2;
}

istream &ExceptionFrame::unserializePart(istream &in)
{

   READ(in, exception);
   READ(in, tid);
   READ(in, from_addr);
   READ(in, to_addr);
   
   return in;
}

#if 0
int main(int argc, char **argv)
{

   StdFrame f;
   f.clearCache();

   f.setCached(2);
   f.setCached(3);
   f.unsetCached(7);
   f.setCached(7);

   for(unsigned int i = 0; i < MAX_VALUES_COUNT; i++) {
      cout << i << ": ";
      if (f.isCached(i))
         cout << "cached.";
      else
         cout << "not cached.";
      cout << endl;
   }

}
#endif
