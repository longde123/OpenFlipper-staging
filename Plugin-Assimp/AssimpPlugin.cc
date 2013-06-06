/*===========================================================================*\
*                                                                            *
*                              OpenFlipper                                   *
*      Copyright (C) 2001-2011 by Computer Graphics Group, RWTH Aachen       *
*                           www.openflipper.org                              *
*                                                                            *
*--------------------------------------------------------------------------- *
*  This file is part of OpenFlipper.                                         *
*                                                                            *
*  OpenFlipper is free software: you can redistribute it and/or modify       *
*  it under the terms of the GNU Lesser General Public License as            *
*  published by the Free Software Foundation, either version 3 of            *
*  the License, or (at your option) any later version with the               *
*  following exceptions:                                                     *
*                                                                            *
*  If other files instantiate templates or use macros                        *
*  or inline functions from this file, or you compile this file and          *
*  link it with other files to produce an executable, this file does         *
*  not by itself cause the resulting executable to be covered by the         *
*  GNU Lesser General Public License. This exception does not however        *
*  invalidate any other reasons why the executable file might be             *
*  covered by the GNU Lesser General Public License.                         *
*                                                                            *
*  OpenFlipper is distributed in the hope that it will be useful,            *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Lesser General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU LesserGeneral Public           *
*  License along with OpenFlipper. If not,                                   *
*  see <http://www.gnu.org/licenses/>.                                       *
*                                                                            *
\*===========================================================================*/

/*===========================================================================*\
*                                                                            *
*   $Revision: 13361 $                                                       *
*   $LastChangedBy: moebius $                                                *
*   $Date: 2012-01-12 16:33:16 +0100 (Thu, 12 Jan 2012) $                     *
*                                                                            *
\*===========================================================================*/




#include "AssimpPlugin.hh"

/*
 * we have to implement our own aiScene and aiMaterial constructor/destructor
 * since the linker cannot find the corresponding symbols from the
 * assimp library
 */

aiMaterial::aiMaterial() {
  mNumAllocated = 0;
  mNumProperties = 0;
  mProperties = NULL;
}
aiMaterial::~aiMaterial() {
  for (unsigned int i = 0; i < mNumProperties; ++i)
    delete mProperties[i];
  delete[] mProperties;
}

aiScene::aiScene() {
    mFlags = 0;
    mRootNode = NULL;
    mNumMeshes = 0;
    mMeshes = NULL;
    mNumMaterials = 0;
    mMaterials = NULL;
    mNumAnimations = 0;
    mAnimations = NULL;
    mNumTextures = 0;
    mTextures = NULL;
    mNumLights = 0;
    mLights = NULL;
    mNumCameras = 0;
    mCameras = NULL;
}
aiScene::~aiScene() {
  delete mRootNode;

  for (unsigned int i = 0; i < mNumMeshes; ++i)
    delete mMeshes[i];
  delete[] mMeshes;

  for (unsigned int i = 0; i < mNumMaterials; ++i)
    delete mMaterials[i];
  delete[] mMaterials;

  for (unsigned int i = 0; i < mNumAnimations; ++i)
    delete mAnimations[i];
  delete[] mAnimations;

  for (unsigned int i = 0; i < mNumTextures; ++i)
    delete mTextures[i];
  delete[] mTextures;

  for (unsigned int i = 0; i < mNumLights; ++i)
    delete mLights[i];
  delete[] mLights;

  for (unsigned int i = 0; i < mNumCameras; ++i)
    delete mCameras[i];
  delete[] mCameras;
}


AssimpPlugin::AssimpPlugin()
  :
  loadOptions_(0),
  saveOptions_(0),
  type_(DATA_TRIANGLE_MESH)
{
}

void AssimpPlugin::initializePlugin() {
}

int AssimpPlugin::convertAiSceneToOpenMesh(const aiScene *_scene, QString _objectName) {
    int objectId = -1;
    emit addEmptyObject(type_, objectId);

    BaseObject* object(0);
    if(!PluginFunctions::getObject( objectId, object )) {
        emit log(LOGERR, tr("Could not create new object!"));
        return -1;
    }

    object->setName(_objectName);

    PolyMeshObject* polyMeshObj = dynamic_cast< PolyMeshObject* > (object);
    TriMeshObject*  triMeshObj  = dynamic_cast< TriMeshObject*  > (object);

    if (polyMeshObj) {
      for (unsigned int i = 0; i < _scene->mNumMeshes; ++i)
        convertPolyMeshToAiMesh(polyMeshObj->mesh(), _scene->mMeshes[i]);

      polyMeshObj->update();
      polyMeshObj->show();

    } else if (triMeshObj) {
      for (unsigned int i = 0; i < _scene->mNumMeshes; ++i)
        convertAiMeshToTriMesh(triMeshObj->mesh(), _scene->mMeshes[i]);

      triMeshObj->update();
      triMeshObj->show();
    }

    emit openedFile( object->id() );

    // Update viewport
    PluginFunctions::viewAll();

    return objectId;
}

bool AssimpPlugin::convertOpenMeshToAiScene(aiScene *_scene, BaseObjectData *_object) {
  _scene->mMeshes = new aiMesh*[1];
  _scene->mMeshes[0] = new aiMesh();
  _scene->mNumMeshes = 1;
  _scene->mRootNode = new aiNode();
  _scene->mRootNode->mNumChildren = 0;
  _scene->mRootNode->mNumMeshes = 1;
  _scene->mRootNode->mMeshes = new unsigned int[1];
  _scene->mRootNode->mMeshes[0] = 0;

  // assimp requires at least one material
  _scene->mMaterials = new aiMaterial*[1];
  _scene->mMaterials[0] = new aiMaterial();
  _scene->mNumMaterials = 1;

  if ( _object->dataType( DATA_POLY_MESH ) ) {
    return convertPolyMeshToAiMesh(dynamic_cast<PolyMeshObject*>(_object)->mesh(), _scene->mMeshes[0]);
  }
  else if ( _object->dataType( DATA_TRIANGLE_MESH ) ) {
    return convertTriMeshToAiMesh(dynamic_cast<TriMeshObject*>(_object)->mesh(), _scene->mMeshes[0]);
  }
  else {
    emit log(LOGERR, tr("Unable to save (object is not a compatible mesh type)"));
    return false;
  }
}

void AssimpPlugin::convertAiMeshToPolyMesh(PolyMesh *_polyMesh, aiMesh *_mesh) {
  mapVertices(_polyMesh, _mesh);

  std::vector<OpenMesh::VertexHandle> vhandles;
  for (unsigned int i = 0; i < _mesh->mNumFaces; ++i) {
    aiFace& face = _mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; ++j) {
      vhandles.push_back(vertexHandles_[face.mIndices[j]]);
      if (_mesh->HasNormals()) {
        aiVector3D& aiNormal = _mesh->mNormals[face.mIndices[j]];
        _polyMesh->set_normal(vhandles.back(), ACG::Vec3d(aiNormal.x, aiNormal.y, aiNormal.z));
      }
    }

    _polyMesh->add_face(vhandles);
    vhandles.clear();
  }

  if (!_mesh->HasNormals())
    _polyMesh->update_normals();
  else
    _polyMesh->update_face_normals();
}

void AssimpPlugin::convertAiMeshToTriMesh(TriMesh *_triMesh, aiMesh *_mesh) {
  mapVertices(_triMesh, _mesh);

  std::vector<OpenMesh::VertexHandle> vhandles;
  for (unsigned int i = 0; i < _mesh->mNumFaces; ++i) {
    aiFace& face = _mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; ++j) {
      vhandles.push_back(vertexHandles_[face.mIndices[j]]);
      if (_mesh->HasNormals()) {
        aiVector3D& aiNormal = _mesh->mNormals[face.mIndices[j]];
        _triMesh->set_normal(vhandles.back(), ACG::Vec3d(aiNormal.x, aiNormal.y, aiNormal.z));
      }
    }

    _triMesh->add_face(vhandles);
    vhandles.clear();
  }

  if (!_mesh->HasNormals())
    _triMesh->update_normals();
  else
    _triMesh->update_face_normals();
}

bool AssimpPlugin::convertPolyMeshToAiMesh(PolyMesh *_polyMesh, aiMesh *_mesh) {
  _mesh->mPrimitiveTypes = aiPrimitiveType_POLYGON;
  _mesh->mNumVertices = _polyMesh->n_vertices();
  _mesh->mNumFaces = _polyMesh->n_faces();

  _mesh->mVertices = new aiVector3D[_mesh->mNumVertices];
  _mesh->mNormals = new aiVector3D[_mesh->mNumVertices];
  _mesh->mFaces = new aiFace[_mesh->mNumFaces];

  std::map<OpenMesh::VertexHandle, int> vertexHandles;
  int i = 0;
  for (PolyMesh::ConstVertexIter v_it = _polyMesh->vertices_begin(); v_it != _polyMesh->vertices_end(); ++v_it, ++i) {
      ACG::Vec3d pos = _polyMesh->point(v_it);
      ACG::Vec3d normal = _polyMesh->normal(v_it);
      _mesh->mVertices[i] = aiVector3D(pos[0], pos[1], pos[2]);
      _mesh->mNormals[i] = aiVector3D(normal[0], normal[1], normal[2]);
      vertexHandles[*v_it] = i;
  }

  i = 0;
  for (PolyMesh::ConstFaceIter f_it = _polyMesh->faces_begin(); f_it != _polyMesh->faces_end(); ++f_it, ++i) {
    int nVertices = 0;
    for (PolyMesh::ConstFaceVertexIter fv_it = _polyMesh->fv_iter(*f_it); fv_it; ++fv_it) {
      ++nVertices;
    }

    _mesh->mFaces[i].mNumIndices = nVertices;
    _mesh->mFaces[i].mIndices = new unsigned int[nVertices];
    int j = 0;
    for (PolyMesh::ConstFaceVertexIter fv_it = _polyMesh->fv_iter(*f_it); fv_it; ++fv_it, ++j) {
      _mesh->mFaces[i].mIndices[j] = vertexHandles[fv_it];
    }
  }

  return true;
}

bool AssimpPlugin::convertTriMeshToAiMesh(TriMesh *_triMesh, aiMesh *_mesh) {
  _mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
  _mesh->mNumVertices = _triMesh->n_vertices();
  _mesh->mNumFaces = _triMesh->n_faces();

  _mesh->mVertices = new aiVector3D[_mesh->mNumVertices];
  _mesh->mNormals = new aiVector3D[_mesh->mNumVertices];
  _mesh->mFaces = new aiFace[_mesh->mNumFaces];

  std::map<OpenMesh::VertexHandle, int> vertexHandles;
  int i = 0;
  for (TriMesh::ConstVertexIter v_it = _triMesh->vertices_begin(); v_it != _triMesh->vertices_end(); ++v_it, ++i) {
      ACG::Vec3d pos = _triMesh->point(v_it);
      ACG::Vec3d normal = _triMesh->normal(v_it);
      _mesh->mVertices[i] = aiVector3D(pos[0], pos[1], pos[2]);
      _mesh->mNormals[i] = aiVector3D(normal[0], normal[1], normal[2]);
      vertexHandles[*v_it] = i;
  }

  i = 0;
  for (TriMesh::ConstFaceIter f_it = _triMesh->faces_begin(); f_it != _triMesh->faces_end(); ++f_it, ++i) {
    _mesh->mFaces[i].mNumIndices = 3;
    _mesh->mFaces[i].mIndices = new unsigned int[3];
    int j = 0;
    for (PolyMesh::ConstFaceVertexIter fv_it = _triMesh->fv_iter(*f_it); fv_it; ++fv_it, ++j) {
      _mesh->mFaces[i].mIndices[j] = vertexHandles[fv_it];
    }
  }

  return true;
}

void AssimpPlugin::mapVertices(PolyMesh *_polyMesh, aiMesh *_mesh) {
  vertexHandles_.clear();

  for (unsigned int i = 0; i < _mesh->mNumVertices; ++i) {
    vertexHandles_[i] = _polyMesh->add_vertex(ACG::Vec3d(_mesh->mVertices[i].x, _mesh->mVertices[i].y, _mesh->mVertices[i].z));
  }
}

void AssimpPlugin::mapVertices(TriMesh *_triMesh, aiMesh *_mesh) {
  vertexHandles_.clear();

  for (unsigned int i = 0; i < _mesh->mNumVertices; ++i) {
    vertexHandles_[i] = _triMesh->add_vertex(ACG::Vec3d(_mesh->mVertices[i].x, _mesh->mVertices[i].y, _mesh->mVertices[i].z));
  }
}

DataType AssimpPlugin::supportedType() {
  DataType type = DATA_POLY_MESH | DATA_TRIANGLE_MESH;
  return type;
}


QString AssimpPlugin::getSaveFilters() {
  return QString( tr("Alias/Wavefront ( *.obj );;Collada ( *.dae );;Stereolithography files ( *.stl );;Polygon File Format files ( *.ply )" ) );
}

QString AssimpPlugin::getLoadFilters() {
  return QString( tr("Alias/Wavefront ( *.obj );;AutoCAD DXF ( *.dxf );;Collada ( *.dae );;Stereolithography files ( *.stl );;Polygon File Format files ( *.ply )") );
}

QWidget *AssimpPlugin::saveOptionsWidget(QString) {
  if (!saveOptions_) {
    saveOptions_ = new QWidget();
  }

  return saveOptions_;
}

QWidget *AssimpPlugin::loadOptionsWidget(QString) {
  if (!loadOptions_) {
    loadOptions_ = new QWidget();
  }

  return loadOptions_;
}

int AssimpPlugin::loadObject(QString _filename) {
  Assimp::Importer importer;

  const aiScene* scene = NULL;
  if (type_ == DATA_TRIANGLE_MESH)
    scene = importer.ReadFile(_filename.toStdString(), aiProcess_JoinIdenticalVertices | aiProcess_Triangulate);
  else
    scene = importer.ReadFile(_filename.toStdString(), aiProcess_JoinIdenticalVertices);

  if (!scene) {
    emit log(LOGERR, tr(importer.GetErrorString()));
    return -1;
  }

  QFileInfo f(_filename);
  return convertAiSceneToOpenMesh(scene, f.fileName());
}

int AssimpPlugin::loadObject(QString _filename, DataType _type) {
  type_ = _type;
  return loadObject(_filename);
}

bool AssimpPlugin::saveObject(int _id, QString _filename) {
  BaseObjectData* object;
  PluginFunctions::getObject(_id,object);

  if (!object) {
    emit log(LOGERR, tr("Could not get the object with the given id"));
    return false;
  }

  object->setFromFileName(_filename);
  object->setName(object->filename());

  aiScene scene;

  if (!convertOpenMeshToAiScene(&scene, object))
    return false;

  Assimp::Exporter exporter;

  QFileInfo f(_filename);

  std::string formatId = (f.suffix() == "dae") ? "collada" : f.suffix().toStdString();
  bool ok = exporter.Export(&scene, formatId, _filename.toStdString()) == AI_SUCCESS;
  if (!ok)
    emit log(LOGERR, exporter.GetErrorString());

  return ok;
}

Q_EXPORT_PLUGIN2( assimpplugin , AssimpPlugin )
