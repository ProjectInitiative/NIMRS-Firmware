#ifndef CVREGISTRY_MOCK_H
#define CVREGISTRY_MOCK_H

struct CvDef {
  uint16_t id;
  const char *name;
  const char *desc;
};

const CvDef CV_DEFS[] = {{1, "Address", "DCC Address"}};
const size_t CV_DEFS_COUNT = 1;

#endif
