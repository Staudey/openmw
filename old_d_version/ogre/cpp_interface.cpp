/*
  OpenMW - The completely unofficial reimplementation of Morrowind
  Copyright (C) 2008  Nicolay Korslund
  Email: < korslund@gmail.com >
  WWW: http://openmw.snaptoad.com/

  This file (cpp_interface.cpp) is part of the OpenMW package.

  OpenMW is distributed as free software: you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 3, as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  version 3 along with this program. If not, see
  http://www.gnu.org/licenses/ .

 */

//-----------------------------------------------------------------------
//               E X P O R T E D    V A R I A B L E S
//-----------------------------------------------------------------------

extern "C"
{
  int lightConst;
  float lightConstValue;

  int lightLinear;
  int lightLinearMethod;
  float lightLinearValue;
  float lightLinearRadiusMult;

  int lightQuadratic;
  int lightQuadraticMethod;
  float lightQuadraticValue;
  float lightQuadraticRadiusMult;

  int lightOutQuadInLin;
}


//-----------------------------------------------------------------------
//               E X P O R T E D    F U N C T I O N S
//-----------------------------------------------------------------------

extern "C" void ogre_cleanup()
{
  // Kill the input systems. This will reset input options such as key
  // repetition.
  mInputManager->destroyInputObject(mKeyboard);
  mInputManager->destroyInputObject(mMouse);
  OIS::InputManager::destroyInputSystem(mInputManager);

  // Code killing ogre has been ported already
}

// Initialize window. This will create and show the actual window.
extern "C" void ogre_initWindow()
{
  TRACE("ogre_initWindow");

  // Initialize OGRE.
  mWindow = mRoot->initialise(true, "OpenMW", "");

  // Set up the input system

  using namespace OIS;

  size_t windowHnd;
  mWindow->getCustomAttribute("WINDOW", &windowHnd);

  std::ostringstream windowHndStr;
  ParamList pl;	

  windowHndStr << windowHnd;
  pl.insert(std::make_pair(std::string("WINDOW"), windowHndStr.str()));

  // Non-exclusive mouse and keyboard input in debug mode
  if(g_isDebug)
    {
#if defined OIS_WIN32_PLATFORM
      pl.insert(std::make_pair(std::string("w32_mouse"),
                               std::string("DISCL_FOREGROUND" )));
      pl.insert(std::make_pair(std::string("w32_mouse"),
                               std::string("DISCL_NONEXCLUSIVE")));
      pl.insert(std::make_pair(std::string("w32_keyboard"),
                               std::string("DISCL_FOREGROUND")));
      pl.insert(std::make_pair(std::string("w32_keyboard"),
                               std::string("DISCL_NONEXCLUSIVE")));
#elif defined OIS_LINUX_PLATFORM
      pl.insert(std::make_pair(std::string("x11_mouse_grab"),
                               std::string("false")));
      pl.insert(std::make_pair(std::string("x11_mouse_hide"),
                               std::string("false")));
      pl.insert(std::make_pair(std::string("x11_keyboard_grab"),
                               std::string("false")));
      pl.insert(std::make_pair(std::string("XAutoRepeatOn"),
                               std::string("true")));
#endif
    }

  mInputManager = InputManager::createInputSystem( pl );

  const bool bufferedKeys = true;
  const bool bufferedMouse = true;

  // Create all devices
  mKeyboard = static_cast<Keyboard*>(mInputManager->createInputObject
                                     ( OISKeyboard, bufferedKeys ));
  mMouse = static_cast<Mouse*>(mInputManager->createInputObject
                               ( OISMouse, bufferedMouse ));

  // Set mouse region
  const MouseState &ms = mMouse->getMouseState();
  ms.width = mWindow->getWidth();
  ms.height = mWindow->getHeight();

  // Register the input listener
  mKeyboard -> setEventCallback( &mInput );
  mMouse    -> setEventCallback( &mInput );
}

// Make a scene
extern "C" void ogre_makeScene()
{
  // Get the SceneManager, in this case a generic one
  mSceneMgr = mRoot->createSceneManager(ST_GENERIC);

  // Create the camera
  mCamera = mSceneMgr->createCamera("PlayerCam");

  mCamera->setNearClipDistance(5);

  // Create one viewport, entire window
  vp = mWindow->addViewport(mCamera);
  // Give the backround a healthy shade of green
  vp->setBackgroundColour(ColourValue(0,0.1,0));

  // Alter the camera aspect ratio to match the viewport
  mCamera->setAspectRatio(Real(vp->getActualWidth()) / Real(vp->getActualHeight()));

  mCamera->setFOVy(Degree(55));

  // Set default mipmap level (NB some APIs ignore this)
  TextureManager::getSingleton().setDefaultNumMipmaps(5);

  // Load resources
  ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

  // Add the frame listener
  mRoot->addFrameListener(&mFrameListener);

  // Turn the entire scene (represented by the 'root' node) -90
  // degrees around the x axis. This makes Z go upwards, and Y go into
  // the screen (when x is to the right.) This is the orientation that
  // Morrowind uses, and it automagically makes everything work as it
  // should.
  SceneNode *rt = mSceneMgr->getRootSceneNode();
  mwRoot = rt->createChildSceneNode();
  mwRoot->pitch(Degree(-90));

  /*
  g_light = mSceneMgr->createLight("carry");
  g_light->setDiffuseColour(1,0.7,0.3);
  g_light->setAttenuation(2000, 0, 0.008, 0);
  */
}

/*
// Toggle carryable light
extern "C" void ogre_toggleCarryLight()
{
  if(g_spotOn == 0)
    {
      g_light->setVisible(true);
      g_spotOn = 1;
    }
  else
    {
      g_light->setVisible(false);
      g_spotOn = 0;
    }
}
*/

// Toggle ambient light
extern "C" void ogre_toggleLight()
{
  if(g_lightOn == 0)
    {
      std::cout << "Turning the lights up\n";
      ColourValue half = 0.7*g_ambient + 0.3*ColourValue(1,1,1);
      mSceneMgr->setAmbientLight(half);
      g_lightOn = 1;
    }
  else if(g_lightOn == 1)
    {
      std::cout << "Turning the lights to full\n";
      g_lightOn = 2;
      mSceneMgr->setAmbientLight(ColourValue(1,1,1));
    }
  else
    {
      std::cout << "Setting lights to normal\n";
      g_lightOn = 0;
      mSceneMgr->setAmbientLight(g_ambient);
    }
}

// Create a sky dome. Currently disabled since we aren't including the
// Ogre example data (which has the sky material.)
extern "C" void ogre_makeSky()
{
  //mSceneMgr->setSkyDome( true, "Examples/CloudySky", 5, 8 );
}

extern "C" Light* ogre_attachLight(char *name, SceneNode* base,
				  float r, float g, float b,
				  float radius)
{
  Light *l = mSceneMgr->createLight(name);
  l->setDiffuseColour(r,g,b);

  radius /= 4.0f;

  float cval=0.0f, lval=0.0f, qval=0.0f;
  if(lightConst)
    cval = lightConstValue;
  if(!lightOutQuadInLin)
  {
    if(lightLinear)
      radius *= lightLinearRadiusMult;
    if(lightQuadratic)
      radius *= lightQuadraticRadiusMult;

    if(lightLinear)
      lval = lightLinearValue / pow(radius, lightLinearMethod);
    if(lightQuadratic)
      qval = lightQuadraticValue / pow(radius, lightQuadraticMethod);
  }
  else
  {
    // FIXME:
    // Do quadratic or linear, depending if we're in an exterior or interior
    // cell, respectively. Ignore lightLinear and lightQuadratic.
  }

  // The first parameter is a cutoff value on which meshes to
  // light. If it's set to small, some meshes will end up 'flashing'
  // in and out of light depending on the camera distance from the
  // light.
  l->setAttenuation(10*radius, cval, lval, qval);

  // base might be null, sometimes lights don't have meshes
  if(base) base->attachObject(l);

  return l;
}

// Toggle between fullscreen and windowed mode.
extern "C" void ogre_toggleFullscreen()
{
  std::cout << "Not implemented yet\n";
}

extern "C" void ogre_setAmbient(float r, float g, float b, // Ambient light
			       float rs, float gs, float bs) // "Sunlight"
{
  g_ambient = ColourValue(r, g, b);
  mSceneMgr->setAmbientLight(g_ambient);

  // Create a "sun" that shines light downwards. It doesn't look
  // completely right, but leave it for now.
  Light *l = mSceneMgr->createLight("Sun");
  l->setDiffuseColour(rs, gs, bs);
  l->setType(Light::LT_DIRECTIONAL);
  l->setDirection(0,-1,0);
}

extern "C" void ogre_setFog(float rf, float gf, float bf, // Fog color
			float flow, float fhigh) // Fog distance
{
  ColourValue fogColor( rf, gf, bf );
  mSceneMgr->setFog( FOG_LINEAR, fogColor, 0.0, flow, fhigh );

  // Don't render what you can't see anyway
  mCamera->setFarClipDistance(fhigh + 10);

  // Leave this out for now
  vp->setBackgroundColour(fogColor);
}

extern "C" void ogre_startRendering()
{
  mRoot->startRendering();
}

// Copy a scene node and all its children
void cloneNode(SceneNode *from, SceneNode *to, char* name)
{
  to->setPosition(from->getPosition());
  to->setOrientation(from->getOrientation());
  to->setScale(from->getScale());

  SceneNode::ObjectIterator it = from->getAttachedObjectIterator();
  while(it.hasMoreElements())
    {
      // We can't handle non-entities.
      Entity *e = dynamic_cast<Entity*> (it.getNext());
      if(e)
        {
          e = e->clone(String(name) + ":" + e->getName());
          to->attachObject(e);
        }
    }

  // Recursively clone all child nodes
  SceneNode::ChildNodeIterator it2 = from->getChildIterator();
  while(it2.hasMoreElements())
    {
      cloneNode((SceneNode*)it2.getNext(), to->createChildSceneNode(), name);
    }
}

// Convert a Morrowind rotation (3 floats) to a quaternion (4 floats)
extern "C" void ogre_mwToQuaternion(float *mw, float *quat)
{
  // Rotate around X axis
  Quaternion xr(Radian(-mw[0]), Vector3::UNIT_X);

  // Rotate around Y axis
  Quaternion yr(Radian(-mw[1]), Vector3::UNIT_Y);

  // Rotate around Z axis
  Quaternion zr(Radian(-mw[2]), Vector3::UNIT_Z);

  // Rotates first around z, then y, then x
  Quaternion res = xr*yr*zr;

  // Copy result back to caller
  for(int i=0; i<4; i++)
    quat[i] = res[i];
}

// Supposed to insert a copy of the node, for now it just inserts the
// actual node.
extern "C" SceneNode *ogre_insertNode(SceneNode *base, char* name,
                                      float *pos, float *quat,
                                      float scale)
{
  //std::cout << "ogre_insertNode(" << name << ")\n";
  SceneNode *node = mwRoot->createChildSceneNode(name);

  // Make a copy of the node
  cloneNode(base, node, name);

  // Apply transformations
  node->setPosition(pos[0], pos[1], pos[2]);
  node->setOrientation(quat[0], quat[1], quat[2], quat[3]);

  node->setScale(scale, scale, scale);

  return node;
}

// Get the world transformation of a node (the total transformation of
// this node and all parent nodes). Return it as a translation
// (3-vector) and a rotation / scaling part (3x3 matrix)
extern "C" void ogre_getWorldTransform(SceneNode *node,
                                       float *trans, // Storage for translation
                                       float *matrix)// For 3x3 matrix
{
  // Get the world transformation first
  Matrix4 trafo;
  node->getWorldTransforms(&trafo);

  // Extract the translation part and pass it to the caller
  Vector3 tr = trafo.getTrans();
  trans[0] = tr[0];
  trans[1] = tr[1];
  trans[2] = tr[2];

  // Next extract the matrix
  Matrix3 mat;
  trafo.extract3x3Matrix(mat);
  matrix[0] = mat[0][0];
  matrix[1] = mat[0][1];
  matrix[2] = mat[0][2];
  matrix[3] = mat[1][0];
  matrix[4] = mat[1][1];
  matrix[5] = mat[1][2];
  matrix[6] = mat[2][0];
  matrix[7] = mat[2][1];
  matrix[8] = mat[2][2];
}

// Create the water plane. It doesn't really resemble "water" yet
// though.
extern "C" void ogre_createWater(float level)
{
    // Create a plane aligned with the xy-plane.
    MeshManager::getSingleton().createPlane("water",
           ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
           Plane(Vector3::UNIT_Z, level),
	   150000,150000
	   );
    Entity *ent = mSceneMgr->createEntity( "WaterEntity", "water" );
    mwRoot->createChildSceneNode()->attachObject(ent);
    ent->setCastShadows(false);
}

// Manual loader for meshes. Reloading individual meshes is too
// difficult, and not worth the trouble. Later I should make one
// loader for each NIF file, and whenever it is invoked it should
// somehow reload the entire file. How this is to be done when some of
// the meshes might be loaded and in use already, I have no
// idea. Let's just ignore it for now.

class MeshLoader : public ManualResourceLoader
{
public:

  void loadResource(Resource *resource)
  {
  }
} dummyLoader;

// Load the contents of a mesh
extern "C" void ogre_createMesh(
		char* name,		// Name of the mesh
		int32_t numVerts,	// Number of vertices
		float* vertices,	// Vertex list
		float* normals,		// Normal list
		float* colors,		// Vertex colors
		float* uvs,		// Texture coordinates
		int32_t numFaces,	// Number of faces*3
		uint16_t* faces,	// Faces
		float radius,		// Bounding sphere
		char* material,		// Material
		// Bounding box
		float minX,float minY,float minZ,
		float maxX,float maxY,float maxZ,
		SceneNode *owner
		)
{
  //std::cerr << "Creating mesh " << name << "\n";

  MeshPtr msh = MeshManager::getSingleton().createManual(name, "Meshes",
							 &dummyLoader);

  Entity *e = mSceneMgr->createEntity(name, name);

  owner->attachObject(e);
  //msh->setSkeletonName(name);

  // Create vertex data structure
  msh->sharedVertexData = new VertexData();
  msh->sharedVertexData->vertexCount = numVerts;

  /// Create declaration (memory format) of vertex data
  VertexDeclaration* decl = msh->sharedVertexData->vertexDeclaration;

  int nextBuf = 0;
  // 1st buffer
  decl->addElement(nextBuf, 0, VET_FLOAT3, VES_POSITION);

  /// Allocate vertex buffer of the requested number of vertices (vertexCount) 
  /// and bytes per vertex (offset)
  HardwareVertexBufferSharedPtr vbuf = 
    HardwareBufferManager::getSingleton().createVertexBuffer(
	VertexElement::getTypeSize(VET_FLOAT3),
        numVerts, HardwareBuffer::HBU_STATIC_WRITE_ONLY);

  /// Upload the vertex data to the card
  vbuf->writeData(0, vbuf->getSizeInBytes(), vertices, true);

  /// Set vertex buffer binding so buffer 0 is bound to our vertex buffer
  VertexBufferBinding* bind = msh->sharedVertexData->vertexBufferBinding; 
  bind->setBinding(nextBuf++, vbuf);

  // The lists are read in the same order that they appear in NIF
  // files, and likely in memory. Sequential reads might possibly
  // avert an occational cache miss.

  // normals
  if(normals)
    {
      //std::cerr << "+ Adding normals\n";
      decl->addElement(nextBuf, 0, VET_FLOAT3, VES_NORMAL);
      vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(
          VertexElement::getTypeSize(VET_FLOAT3),
          numVerts, HardwareBuffer::HBU_STATIC_WRITE_ONLY);

      vbuf->writeData(0, vbuf->getSizeInBytes(), normals, true);

      bind->setBinding(nextBuf++, vbuf);       
    }

  // vertex colors
  if(colors)
    {
      //std::cerr << "+ Adding vertex colors\n";
      // Use render system to convert colour value since colour packing varies
      RenderSystem* rs = Root::getSingleton().getRenderSystem();
      RGBA colorsRGB[numVerts];
      RGBA *pColour = colorsRGB;
      for(int i=0; i<numVerts; i++)
	{
	  rs->convertColourValue(ColourValue(colors[0],colors[1],colors[2], colors[3]),
				 pColour++);
	  colors += 4;
	}

      decl->addElement(nextBuf, 0, VET_COLOUR, VES_DIFFUSE);
      /// Allocate vertex buffer of the requested number of vertices (vertexCount) 
      /// and bytes per vertex (offset)
      vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(
          VertexElement::getTypeSize(VET_COLOUR),
	  numVerts, HardwareBuffer::HBU_STATIC_WRITE_ONLY);
      /// Upload the vertex data to the card
      vbuf->writeData(0, vbuf->getSizeInBytes(), colorsRGB, true);

      /// Set vertex buffer binding so buffer 1 is bound to our colour buffer
      bind->setBinding(nextBuf++, vbuf);
    }

  if(uvs)
    {
      //std::cerr << "+ Adding texture coordinates\n";
      decl->addElement(nextBuf, 0, VET_FLOAT2, VES_TEXTURE_COORDINATES);
      vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(
          VertexElement::getTypeSize(VET_FLOAT2),
          numVerts, HardwareBuffer::HBU_STATIC_WRITE_ONLY);

      vbuf->writeData(0, vbuf->getSizeInBytes(), uvs, true);

      bind->setBinding(nextBuf++, vbuf);       
    }

  // Create the submesh that holds triangle data
  SubMesh* sub = msh->createSubMesh(name);
  sub->useSharedVertices = true;

  if(numFaces)
    {
      //std::cerr << "+ Adding faces\n";
      /// Allocate index buffer of the requested number of faces
      HardwareIndexBufferSharedPtr ibuf = HardwareBufferManager::getSingleton().
	createIndexBuffer(
			  HardwareIndexBuffer::IT_16BIT, 
			  numFaces,
			  HardwareBuffer::HBU_STATIC_WRITE_ONLY);

      /// Upload the index data to the card
      ibuf->writeData(0, ibuf->getSizeInBytes(), faces, true);

      /// Set parameters of the submesh
      sub->indexData->indexBuffer = ibuf;
      sub->indexData->indexCount = numFaces;
      sub->indexData->indexStart = 0;
    }

  // Create a material with the given texture, if any.

  // If this mesh has a material, attach it.
  if(material) sub->setMaterialName(name);

  /*
  // Assign this submesh to the given bone
  VertexBoneAssignment v;
  v.boneIndex = ((Bone*)bone)->getHandle();
  v.weight = 1.0;

  std::cerr << "+ Assigning bone index " << v.boneIndex << "\n";

  for(int i=0; i < numVerts; i++)
    {
      v.vertexIndex = i;
      sub->addBoneAssignment(v);
    }
  */
  /// Set bounding information (for culling)
  msh->_setBounds(AxisAlignedBox(minX,minY,minZ,maxX,maxY,maxZ));

  //std::cerr << "+ Radius: " << radius << "\n";
  msh->_setBoundingSphereRadius(radius);
}

extern "C" void ogre_createMaterial(char *name,	    // Name to give
						    // resource

				   float *ambient,  // Ambient RBG
						    // value
				   float *diffuse,
				   float *specular,
				   float *emissive, // Self
						    // illumination

				   float glossiness,// Same as
						    // shininess?

				   float alpha,     // Use this in all
						    // alpha values?

				   char* texture,   // Texture

                                   int32_t alphaFlags,
                                   uint8_t alphaTest) // Alpha settings
{
      MaterialPtr material = MaterialManager::getSingleton().create(
        name,
	ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

      // This assigns the texture to this material. If the texture
      // name is a file name, and this file exists (in a resource
      // directory), it will automatically be loaded when needed. If
      // not, we should already have inserted a manual loader for the texture.
      if(texture)
        {
          Pass *pass = material->getTechnique(0)->getPass(0);
          TextureUnitState *txt = pass->createTextureUnitState(texture);

          // Add transparencly.

          if(alphaFlags != -1)
            {
              // The 237 alpha flags are by far the most common. Check
              // NiAlphaProperty in nif/properties.d if you need to
              // decode other values. 237 basically means normal
              // transparencly.
              if(alphaFlags == 237)
                {
                  // Enable transparency
                  pass->setSceneBlending(SBT_TRANSPARENT_ALPHA);

                  //pass->setDepthCheckEnabled(false);
                  pass->setDepthWriteEnabled(false);
                }
              else
                std::cout << "UNHANDLED ALPHA FOR " << texture << ": " << alphaFlags << "\n";
            }
        }

      // Set bells and whistles
      material->setAmbient(ambient[0], ambient[1], ambient[2]);
      material->setDiffuse(diffuse[0], diffuse[1], diffuse[2], alpha);
      material->setSpecular(specular[0], specular[1], specular[2], alpha);
      material->setSelfIllumination(emissive[0], emissive[1], emissive[2]);
      material->setShininess(glossiness);
}

extern "C" SceneNode *ogre_getDetachedNode()
{
  SceneNode *node = mwRoot->createChildSceneNode();
  mwRoot->removeChild(node);
  return node;
}

extern "C" SceneNode* ogre_createNode(
		char *name,
		float *trafo,
		SceneNode *parent,
		int32_t noRot)
{
  //std::cout << "ogre_createNode(" << name << ")";
  SceneNode *node = parent->createChildSceneNode(name);
  //std::cout << " ... done\n";

  // First is the translation vector

  // TODO should be "if(!noRot)" only for exterior cells!? Yay for
  // consistency. Apparently, the displacement of the base node in NIF
  // files must be ignored for meshes in interior cells, but not for
  // exterior cells. Or at least that's my hypothesis, and it seems
  // work. There might be some other NIF trickery going on though, you
  // never know when you're reverse engineering someone else's file
  // format. We will handle this later.
  if(!noRot)
    node->setPosition(trafo[0], trafo[1], trafo[2]);

  // Then a 3x3 rotation matrix.
  if(!noRot)
    node->setOrientation(Quaternion(Matrix3(trafo[3], trafo[4], trafo[5],
					    trafo[6], trafo[7], trafo[8],
					    trafo[9], trafo[10], trafo[11]
					    )));

  // Scale is at the end
  node->setScale(trafo[12],trafo[12],trafo[12]);

  return node;
}

/* Code currently not in use

// Insert a raw RGBA image into the texture system.
extern "C" void ogre_insertTexture(char* name, uint32_t width, uint32_t height, void *data)
{
  TexturePtr texture = TextureManager::getSingleton().createManual(
      name, 		// name
      "General",	// group
      TEX_TYPE_2D,     	// type
      width, height,    // width & height
      0,                // number of mipmaps
      PF_BYTE_RGBA,     // pixel format
      TU_DEFAULT);      // usage; should be TU_DYNAMIC_WRITE_ONLY_DISCARDABLE for
                        // textures updated very often (e.g. each frame)

  // Get the pixel buffer
  HardwarePixelBufferSharedPtr pixelBuffer = texture->getBuffer();

  // Lock the pixel buffer and get a pixel box
  pixelBuffer->lock(HardwareBuffer::HBL_NORMAL); // for best performance use HBL_DISCARD!
  const PixelBox& pixelBox = pixelBuffer->getCurrentLock();

  void *dest = pixelBox.data;

  // Copy the data
  memcpy(dest, data, width*height*4);

  // Unlock the pixel buffer
  pixelBuffer->unlock();
}

// We need this later for animated meshes.
extern "C" void* ogre_setupSkeleton(char* name)
{
  SkeletonPtr skel = SkeletonManager::getSingleton().create(
    name, "Closet", true);

  skel->load();

  // Create all bones at the origin and unrotated. This is necessary
  // since our submeshes each have their own model space. We must
  // move the bones after creating an entity, then copy this entity.
  return (void*)skel->createBone();  
}

extern "C" void *ogre_insertBone(char* name, void* rootBone, int32_t index)
{
  return (void*) ( ((Bone*)rootBone)->createChild(index) );
}
*/
