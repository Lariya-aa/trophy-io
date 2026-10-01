#!/usr/bin/env python3
"""Synthetic installed-game and user trophy catalogue coverage; no device access."""
import ctypes as C
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
WRITE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_void_p,C.c_size_t)
class Entry(C.Structure):
    _fields_=[('directory',C.c_char*256),('title',C.c_char*96),('title_id',C.c_char*16),('sources',C.c_uint),('data_files',C.c_uint),('config_valid',C.c_int),('config_error',C.c_int),('data_error',C.c_int)]
class File(C.Structure):
    _fields_=[('path',C.c_char*512),('owner',C.c_char*256),('title_id',C.c_char*16),('original_path',C.c_char*512),('sha256',C.c_char*65),('bytes',C.c_uint64),('source',C.c_uint)]
class Catalog(C.Structure):
    _fields_=[('entries',C.POINTER(Entry)),('count',C.c_size_t),('capacity',C.c_size_t),('system_entries',C.c_size_t),('files',C.POINTER(File)),('file_count',C.c_size_t),('file_capacity',C.c_size_t),('apps_scanned',C.c_size_t),('scan_error',C.c_int)]
class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='trophy-catalog-test-');library=Path(cls.temp.name)/'catalog.so'
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(ROOT/'include'),str(ROOT/'src/catalog.c'),str(ROOT/'src/core.c'),'-o',str(library)],check=True)
        cls.lib=C.CDLL(str(library))
        cls.lib.trophy_catalog_add.argtypes=[C.POINTER(Catalog),C.c_char_p,C.c_uint]
        cls.lib.trophy_catalog_free.argtypes=[C.POINTER(Catalog)]
        cls.lib.trophy_catalog_sort.argtypes=[C.POINTER(Catalog)]
        cls.lib.trophy_catalog_exportable.argtypes=[C.POINTER(Entry)]
        cls.lib.trophy_catalog_state.argtypes=[C.POINTER(Entry)];cls.lib.trophy_catalog_state.restype=C.c_char_p
        cls.lib.trophy_catalog_add_file.argtypes=[C.POINTER(Catalog),C.c_char_p,C.c_char_p,C.c_char_p,C.c_uint64,C.c_uint]
        cls.lib.trophy_catalog_write.argtypes=[C.POINTER(Catalog),WRITE,C.c_void_p]
        cls.lib.trophy_sfo_text.argtypes=[C.c_char_p,C.c_size_t,C.c_char_p,C.c_void_p,C.c_size_t]
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):self.catalog=Catalog()
    def tearDown(self):self.lib.trophy_catalog_free(C.byref(self.catalog))
    def add(self,name,source):return self.lib.trophy_catalog_add(C.byref(self.catalog),name.encode(),source)
    def test_unplayed_installed_games_across_title_prefixes(self):
        for i,tid in enumerate(['PCSA00001','PCSB00001','PCSC00001','PCSE00001','PCSF00001','PCSG00001','PCSH00001','CUSTOM001']):
            owner=f'NPWR{i:05}_00';self.assertEqual(self.add(owner,4),0)
            self.assertEqual(self.lib.trophy_catalog_add_file(C.byref(self.catalog),f'ux0:app/{tid}/sce_sys/trophy/{owner}/TROPHY.TRP'.encode(),owner.encode(),tid.encode(),5936512,4),0)
        self.assertEqual(self.catalog.count,8);self.assertEqual(self.catalog.file_count,8)
        for entry in self.catalog.entries[:8]:
            self.assertEqual(self.lib.trophy_catalog_state(C.byref(entry)),b'Game files')
            self.assertFalse(self.lib.trophy_catalog_exportable(C.byref(entry)))
    def test_union_retains_more_than_128_games_and_user_only_sets(self):
        for i in reversed(range(260)):self.assertEqual(self.add(f'NPWR{i:05}_00',4),0)
        for i in range(31):self.add(f'NPWR{i:05}_00',2)
        for i in range(3):self.add(f'NPWR{i:05}_00',1)
        self.add('NPWR99999_00',2);self.lib.trophy_catalog_sort(C.byref(self.catalog))
        self.assertEqual(self.catalog.count,261)
        self.assertEqual(self.catalog.entries[0].sources,7)
        self.assertEqual(self.catalog.entries[259].directory,b'NPWR00259_00')
        self.assertEqual(self.catalog.entries[260].directory,b'NPWR99999_00')
    def test_personal_export_requires_pair_config_and_valid_identity(self):
        self.add('NPWR16080_00',1);self.add('NPWR16080_00',2);self.add('NPWR16080_00',4)
        e=self.catalog.entries[0];e.config_valid=1;e.data_files=3
        self.assertTrue(self.lib.trophy_catalog_exportable(C.byref(e)))
        for field,value in [('data_files',1),('config_valid',0),('data_error',-123),('config_error',-124)]:
            old=getattr(e,field);setattr(e,field,value)
            self.assertFalse(self.lib.trophy_catalog_exportable(C.byref(e)));setattr(e,field,old)
    def test_config_and_data_orphans_are_visible(self):
        self.add('NPWR00001_00',2);self.add('NPWR00002_00',1)
        self.catalog.entries[0].config_valid=1
        self.assertEqual(self.lib.trophy_catalog_state(C.byref(self.catalog.entries[0])),b'Config only')
        self.assertEqual(self.lib.trophy_catalog_state(C.byref(self.catalog.entries[1])),b'No config')
    def test_unknown_ids_retained_but_never_sent_to_mount(self):
        self.add('GBLN00001_00',4);self.add('PCSG00001',1)
        self.assertEqual(self.catalog.count,2)
        for entry in self.catalog.entries[:2]:self.assertFalse(self.lib.trophy_catalog_exportable(C.byref(entry)))
    def test_system_state_and_path_traversal_are_not_game_entries(self):
        self.assertEqual(self.add('sce_trop',1),0);self.assertEqual(self.catalog.count,0);self.assertEqual(self.catalog.system_entries,1)
        for value in ['..','.','../NPWR00001_00','ux0:evil','evil\\name','x'*256]:self.assertLess(self.add(value,1),0)
    def test_shared_ids_preserve_both_game_paths_and_patch_variants(self):
        for base,tid in [('ux0:app','PCSE00001'),('ux0:app','PCSH00001'),('ux0:patch','PCSE00001')]:
            self.add('NPWR00001_00',4)
            self.lib.trophy_catalog_add_file(C.byref(self.catalog),f'{base}/{tid}/sce_sys/trophy/NPWR00001_00/TROPHY.TRP'.encode(),b'NPWR00001_00',tid.encode(),1024,4)
        self.assertEqual(self.catalog.count,1);self.assertEqual(self.catalog.file_count,3)
    def test_inventory_json_escapes_names_and_marks_partial_scans(self):
        self.add('NPWR00001_00',4);self.catalog.entries[0].title='Quote " slash \\ newline\n日本語'.encode()
        self.catalog.scan_error=-123;self.catalog.apps_scanned=308
        capture=bytearray()
        @WRITE
        def write(context,data,size):
            n=min(size,3);capture.extend(C.string_at(data,n));return n
        self.assertEqual(self.lib.trophy_catalog_write(C.byref(self.catalog),write,None),0)
        report=json.loads(capture);self.assertFalse(report['complete']);self.assertEqual(report['scan_error'],-123)
        self.assertEqual(report['entries'][0]['title'],'Quote " slash \\ newline\n日本語')
    def test_inventory_write_failure_is_propagated(self):
        @WRITE
        def write(context,data,size):return -77
        self.assertEqual(self.lib.trophy_catalog_write(C.byref(self.catalog),write,None),-77)
    def test_sfo_bounds_and_text_type(self):
        value=b'Fate/stay night\0';keys=b'TITLE\0'
        data=struct.pack('<4s4I',b'\0PSF',0x101,36,42,1)+struct.pack('<HHIII',0,0x204,len(value),len(value),0)+keys+value
        out=C.create_string_buffer(96)
        self.assertEqual(self.lib.trophy_sfo_text(data,len(data),b'TITLE',out,len(out)),0);self.assertEqual(out.value,value[:-1])
        for candidate in [data[:-1],data[:10],data[:8]+struct.pack('<I',0xffffffff)+data[12:]]:
            self.assertLess(self.lib.trophy_sfo_text(candidate,len(candidate),b'TITLE',out,len(out)),0)
        self.assertLess(self.lib.trophy_sfo_text(data,len(data),b'TITLE',out,4),0)
if __name__=='__main__':unittest.main(verbosity=2)
