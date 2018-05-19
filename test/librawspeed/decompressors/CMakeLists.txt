FILE(GLOB RAWSPEED_TEST_SOURCES
  "AbstractHuffmanTableTest.cpp"
  "BinaryHuffmanTreeTest.cpp"
  "HuffmanTableTest.cpp"
)

foreach(IN ${RAWSPEED_TEST_SOURCES})
  add_rs_test(${IN})
endforeach()
