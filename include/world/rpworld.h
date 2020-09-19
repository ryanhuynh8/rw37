// Manually crafted main world include file.

#ifndef _RENDERWARE_WORLD_COMPONENT_
#define _RENDERWARE_WORLD_COMPONENT_

#include "babinwor.h"
#include "baclump.h"
#include "bageomet.h"
#include "balight.h"
#include "bamateri.h"
#include "bamatlst.h"
#include "bamesh.h"
#include "bameshop.h"
#include "basector.h"
#include "baworld.h"
#include "baworobj.h"

// Pipe includes.
#include "pipe/p2/bapipew.h"
#ifdef RWDRIVER_D3D9
#include "pipe/p2/d3d9/D3D9pipe.h"
#include "pipe/p2/d3d9/D3D9lights.h"
#include "pipe/p2/d3d9/d3d9vertexshader.h"
#include "pipe/p2/d3d9/nodeD3D9AtomicAllInOne.h"
#include "pipe/p2/d3d9/nodeD3D9SubmitNoLight.h"
#include "pipe/p2/d3d9/nodeD3D9WorldSectorAllInOne.h"
#elif defined(RWDRIVER_D3D8)
#include "pipe/p2/d3d8/D3D8pipe.h"
#include "pipe/p2/d3d8/D3D8lights.h"
//#include "pipe/p2/d3d8/d3d8vertexshader.h"
#include "pipe/p2/d3d8/nodeD3D8AtomicAllInOne.h"
#include "pipe/p2/d3d8/nodeD3D8SubmitNoLight.h"
#include "pipe/p2/d3d8/nodeD3D8WorldSectorAllInOne.h"
#endif //DRIVER SELECTOR

#endif //_RENDERWARE_WORLD_COMPONENT_