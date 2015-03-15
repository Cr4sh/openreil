import base64
import pymongo
from pyopenreil import REIL
from cachetools import LRUCache

_U64IN = lambda v: -(0xFFFFFFFFFFFFFFFFL + 1L - v) if v > 0x7FFFFFFFFFFFFFFFL else v

_U64OUT = lambda v: (0xFFFFFFFFFFFFFFFFL + 1L + v) if v < 0 else v

class CodeStorageMongo(REIL.CodeStorageMem):

    # mongodb host
    DEF_HOST = '127.0.0.1'
    DEF_PORT = 27017

    # defult database name
    DEF_DB = 'openreil'

    # index for instructions collection
    INDEX = [('addr', pymongo.ASCENDING), ('inum', pymongo.ASCENDING)]

    CACHE_SIZE = 1024

    def __init__(self, arch, collection, db = None, host = None, port = None):
        
        self.arch = arch
        self.db_name = self.DEF_DB if db is None else db
        self.collection_name = collection

        self.host = self.DEF_HOST if host is None else host
        self.port = self.DEF_PORT if port is None else port

        # instructions cache
        self.cache = LRUCache(maxsize = self.CACHE_SIZE)

        # connect to the server
        self.client = pymongo.Connection(self.host, self.port)
        self.db = self.client[self.db_name]

        # get collection        
        self.collection = self.db[self.collection_name]
        self.collection.ensure_index(self.INDEX)     

    def __iter__(self):

        for item in self.collection.find().sort(self.INDEX): 

            yield REIL.Insn(self._insn_from_item(item))

    def _insn_to_item(self, insn):

        insn = REIL.Insn(insn)        

        def _arg_in(arg):

            if arg.type == REIL.A_NONE:  

                return ()
            
            elif arg.type == REIL.A_CONST:

                return ( arg.type, arg.size, _U64IN(arg.val) )
            
            else:

                return ( arg.type, arg.size, arg.name )

        if insn.has_attr(REIL.IATTR_BIN):

            # JSON doesn't support binary data
            insn.set_attr(REIL.IATTR_BIN, base64.b64encode(insn.get_attr(REIL.IATTR_BIN)))

        # JSON doesn't support numeric keys
        attr = [ (key, val) for key, val in insn.attr.items() ]

        return {

            'addr': _U64IN(insn.addr), 'size': insn.size, 'inum': insn.inum, 'op': insn.op, \
            'a': _arg_in(insn.a), 'b': _arg_in(insn.b), 'c': _arg_in(insn.c), \
            'attr': attr
        }

    def _insn_from_item(self, item):

        attr, attr_dict = item['attr'], {}

        def _arg_out(arg):

            if len(arg) == 0: 

                return ()

            elif REIL.Arg_type(arg) == REIL.A_CONST:

                arg = ( REIL.Arg_type(arg), REIL.Arg_size(arg), _U64OUT(REIL.Arg_val(arg)) )

            return arg                

        for key, val in attr:

            attr_dict[key] = val

        if attr_dict.has_key(REIL.IATTR_BIN):

            # get instruction binary data from base64
            attr_dict[REIL.IATTR_BIN] = base64.b64decode(attr_dict[REIL.IATTR_BIN])

        return ( 

            ( _U64OUT(item['addr']), item['size'] ), item['inum'], item['op'], \
            ( _arg_out(item['a']), _arg_out(item['b']), _arg_out(item['c']) ), \
            attr_dict
        ) 

    def _get_key(self, ir_addr):

        return { 'addr': ir_addr[0], 'inum': ir_addr[1] }

    def _find(self, ir_addr):

        return self.collection.find_one(self._get_key(ir_addr))

    def _get_insn(self, ir_addr): 

        # get item from cache
        try: return self.cache[ir_addr]
        except KeyError: pass

        # get item from collection
        insn = self._find(ir_addr)
        if insn is not None: 

            insn = self._insn_from_item(insn)

            # update cache
            self.cache[ir_addr] = insn

            return insn

        else:

            raise REIL.StorageError(*ir_addr)

    def _del_insn(self, ir_addr):

        insn = self._find(ir_addr)
        if insn is not None: 

            # remove item from collection
            self.collection.remove(self._get_key(ir_addr))

            # remove item from cache
            try: del self.cache[ir_addr]
            except KeyError: pass

        else:

            raise REIL.StorageError(*ir_addr)

    def _put_insn(self, insn):

        ir_addr = REIL.Insn_ir_addr(insn)

        if self._find(ir_addr) is not None:

            # update existing item
            self.collection.update(self._get_key(ir_addr), self._insn_to_item(insn))            

        else:

            # add a new item
            self.collection.insert(self._insn_to_item(insn))

        # update cache
        self.cache[ir_addr] = insn

    def size(self): 

        return self.collection.find().count()

    def clear(self): 

        self.cache.clear()

        # remove all items of collection
        return self.collection.remove()


#
# EoF
#
