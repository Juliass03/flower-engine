#pragma once
#include "../core/core.h"
#include "glm/ext.hpp"
#include <sstream>

namespace engine{ namespace asset_system{ 

// 头定义直接使用benikabocha的saba库
// See https://github.com/benikabocha/saba/blob/master/src/Saba/Model/MMD/PMXFile.h
// Copyright(c) 2016-2017 benikabocha.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
struct PMXHeader
{
	char	m_magic[4];
	float	m_version;

	uint8_t	m_dataSize;

	uint8_t	m_encode;	//0:UTF16 1:UTF8
	uint8_t	m_addUVNum;

	uint8_t	m_vertexIndexSize;
	uint8_t	m_textureIndexSize;
	uint8_t	m_materialIndexSize;
	uint8_t	m_boneIndexSize;
	uint8_t	m_morphIndexSize;
	uint8_t	m_rigidbodyIndexSize;

	std::string get_header()
	{
		std::stringstream ss;

		ss << "version: " << std::to_string(m_version) << ", " << std::endl;
		ss << "data size: " << std::to_string(m_dataSize) << ", "<< std::endl;

		auto encode_str = m_encode == 1 ? "utf-8" : "utf-16";
		ss << "m_encode: " << encode_str << ", "<< std::endl;
		ss << "vertex index size: " << std::to_string(m_vertexIndexSize) << ", "<< std::endl;
		ss << "texture index size: " << std::to_string(m_textureIndexSize) << ", "<< std::endl;
		ss << "material index size: " << std::to_string(m_materialIndexSize) << ", "<< std::endl;
		ss << "bone index size: " << std::to_string(m_boneIndexSize) << ", "<< std::endl;
		ss << "morph index size: " << std::to_string(m_morphIndexSize) << ", "<< std::endl;
		ss << "rigidbody index size: " << std::to_string(m_rigidbodyIndexSize) << ", "<< std::endl;

		return ss.str();
	}
};

struct PMXInfo
{
	std::string	m_modelName;
	std::string	m_englishModelName;
	std::string	m_comment;
	std::string	m_englishComment;
};

/*
BDEF1
m_boneIndices[0]
BDEF2
m_boneIndices[0-1]
m_boneWeights[0]
BDEF4
m_boneIndices[0-3]
m_boneWeights[0-3]
SDEF
m_boneIndices[0-1]
m_boneWeights[0]
m_sdefC
m_sdefR0
m_sdefR1
QDEF
m_boneIndices[0-3]
m_boneWeights[0-3]
*/
enum class PMXVertexWeight : uint8_t
{
	BDEF1,
	BDEF2,
	BDEF4,
	SDEF,
	QDEF,
};

struct PMXVertex
{
	glm::vec3	m_position;
	glm::vec3	m_normal;
	glm::vec2	m_uv;

	glm::vec4	m_addUV[4];

	PMXVertexWeight	m_weightType; // 0:BDEF1 1:BDEF2 2:BDEF4 3:SDEF 4:QDEF
	int32_t		m_boneIndices[4];
	float		m_boneWeights[4];
	glm::vec3	m_sdefC;
	glm::vec3	m_sdefR0;
	glm::vec3	m_sdefR1;

	float	m_edgeMag;

	std::string get_header()
	{
		std::stringstream ss;


		ss << "pos: " << glm::to_string(m_position) << ", \t";
		ss << "norm: " << glm::to_string(m_normal) << ", \t";
		ss << "uv: " << glm::to_string(m_uv) << ", \t";
		ss << "add uv[0]: " << glm::to_string(m_addUV[0]) << ", \t";
		ss << "add uv[1]: " << glm::to_string(m_addUV[1]) << ", \t";
		ss << "add uv[2]: " << glm::to_string(m_addUV[2]) << ", \t";
		ss << "add uv[3]: " << glm::to_string(m_addUV[3]) << ", \t";

		return ss.str();
	}
};

struct PMXFace
{
	uint32_t m_vertices[3];
};

struct PMXTexture
{
	std::string m_textureName;
};

/*
0x01:両面描画
0x02:地面影
0x04:セルフシャドウマップへの描画
0x08:セルフシャドウの描画
0x10:エッジ描画
0x20:頂点カラー(※2.1拡張)
0x40:Point描画(※2.1拡張)
0x80:Line描画(※2.1拡張)
*/
enum class PMXDrawModeFlags : uint8_t
{
	BothFace = 0x01,
	GroundShadow = 0x02,
	CastSelfShadow = 0x04,
	RecieveSelfShadow = 0x08,
	DrawEdge = 0x10,
	VertexColor = 0x20,
	DrawPoint = 0x40,
	DrawLine = 0x80,
};

/*
0:無効
1:乗算
2:加算
3:サブテクスチャ(追加UV1のx,yをUV参照して通常テクスチャ描画を行う)
*/
enum class PMXSphereMode : uint8_t
{
	None,
	Mul,
	Add,
	SubTexture,
};

enum class PMXToonMode : uint8_t
{
	Separate,	//!< 0:個別Toon
	Common,		//!< 1:共有Toon[0-9] toon01.bmp～toon10.bmp
};

struct PMXMaterial
{
	std::string	m_name;
	std::string	m_englishName;

	glm::vec4	m_diffuse;
	glm::vec3	m_specular;
	float		m_specularPower;
	glm::vec3	m_ambient;

	PMXDrawModeFlags m_drawMode;

	glm::vec4	m_edgeColor;
	float		m_edgeSize;

	int32_t	m_textureIndex;
	int32_t	m_sphereTextureIndex;
	PMXSphereMode m_sphereMode;

	PMXToonMode	m_toonMode;
	int32_t		m_toonTextureIndex;

	std::string	m_memo;

	int32_t	m_numFaceVertices;
};

/*
0x0001  : 接続先(PMD子ボーン指定)表示方法 -> 0:座標オフセットで指定 1:ボーンで指定
0x0002  : 回転可能
0x0004  : 移動可能
0x0008  : 表示
0x0010  : 操作可
0x0020  : IK
0x0080  : ローカル付与 | 付与対象 0:ユーザー変形値／IKリンク／多重付与 1:親のローカル変形量
0x0100  : 回転付与
0x0200  : 移動付与
0x0400  : 軸固定
0x0800  : ローカル軸
0x1000  : 物理後変形
0x2000  : 外部親変形
*/
enum class PMXBoneFlags : uint16_t
{
	TargetShowMode = 0x0001,
	AllowRotate = 0x0002,
	AllowTranslate = 0x0004,
	Visible = 0x0008,
	AllowControl = 0x0010,
	IK = 0x0020,
	AppendLocal = 0x0080,
	AppendRotate = 0x0100,
	AppendTranslate = 0x0200,
	FixedAxis = 0x0400,
	LocalAxis = 0x800,
	DeformAfterPhysics = 0x1000,
	DeformOuterParent = 0x2000,
};

struct PMXIKLink
{
	int32_t			m_ikBoneIndex;
	unsigned char	m_enableLimit;

	//m_enableLimitが1のときのみ
	glm::vec3	m_limitMin;	//ラジアンで表現
	glm::vec3	m_limitMax;	//ラジアンで表現
};

struct PMXBone
{
	std::string	m_name;
	std::u16string m_u16name;
	std::string	m_englishName;

	glm::vec3	m_position;
	int32_t		m_parentBoneIndex;
	int32_t		m_deformDepth;

	PMXBoneFlags	m_boneFlag;

	glm::vec3	m_positionOffset;	//接続先:0の場合
	int32_t		m_linkBoneIndex;	//接続先:1の場合

									//「回転付与」または「移動付与」が有効のみ
	int32_t	m_appendBoneIndex;
	float	m_appendWeight;

	//「軸固定」が有効のみ
	glm::vec3	m_fixedAxis;

	//「ローカル軸」が有効のみ
	glm::vec3	m_localXAxis;
	glm::vec3	m_localZAxis;

	//「外部親変形」が有効のみ
	int32_t	m_keyValue;

	//「IK」が有効のみ
	int32_t	m_ikTargetBoneIndex;
	int32_t	m_ikIterationCount;
	float	m_ikLimit;	//ラジアンで表現

	std::vector<PMXIKLink>	m_ikLinks;
};


/*
0:グループ
1:頂点
2:ボーン,
3:UV,
4:追加UV1
5:追加UV2
6:追加UV3
7:追加UV4
8:材質
9:フリップ(※2.1拡張)
10:インパルス(※2.1拡張)
*/
enum class PMXMorphType : uint8_t
{
	Group,
	Position,
	Bone,
	UV,
	AddUV1,
	AddUV2,
	AddUV3,
	AddUV4,
	Material,
	Flip,
	Impluse,
};

struct PMXMorph
{
	std::string	m_name;
	std::string	m_englishName;

	uint8_t			m_controlPanel;	//1:眉(左下) 2:目(左上) 3:口(右上) 4:その他(右下)  | 0:システム予約
	PMXMorphType	m_morphType;

	struct PositionMorph
	{
		int32_t		m_vertexIndex;
		glm::vec3	m_position;
	};

	struct UVMorph
	{
		int32_t		m_vertexIndex;
		glm::vec4	m_uv;
	};

	struct BoneMorph
	{
		int32_t		m_boneIndex;
		glm::vec3	m_position;
		glm::quat	m_quaternion;
	};

	struct MaterialMorph
	{
		enum class OpType : uint8_t
		{
			Mul,
			Add,
		};

		int32_t		m_materialIndex;
		OpType		m_opType;	//0:乗算 1:加算
		glm::vec4	m_diffuse;
		glm::vec3	m_specular;
		float		m_specularPower;
		glm::vec3	m_ambient;
		glm::vec4	m_edgeColor;
		float		m_edgeSize;
		glm::vec4	m_textureFactor;
		glm::vec4	m_sphereTextureFactor;
		glm::vec4	m_toonTextureFactor;
	};

	struct GroupMorph
	{
		int32_t	m_morphIndex;
		float	m_weight;
	};

	struct FlipMorph
	{
		int32_t	m_morphIndex;
		float	m_weight;
	};

	struct ImpulseMorph
	{
		int32_t		m_rigidbodyIndex;
		uint8_t		m_localFlag;	//0:OFF 1:ON
		glm::vec3	m_translateVelocity;
		glm::vec3	m_rotateTorque;
	};

	std::vector<PositionMorph>	m_positionMorph;
	std::vector<UVMorph>		m_uvMorph;
	std::vector<BoneMorph>		m_boneMorph;
	std::vector<MaterialMorph>	m_materialMorph;
	std::vector<GroupMorph>		m_groupMorph;
	std::vector<FlipMorph>		m_flipMorph;
	std::vector<ImpulseMorph>	m_impulseMorph;
};

struct PMXDispalyFrame
{

	std::string	m_name;
	std::string	m_englishName;

	enum class TargetType : uint8_t
	{
		BoneIndex,
		MorphIndex,
	};
	struct Target
	{
		TargetType	m_type;
		int32_t		m_index;
	};

	enum class FrameType : uint8_t
	{
		DefaultFrame,	//!< 0:通常枠
		SpecialFrame,	//!< 1:特殊枠
	};

	FrameType			m_flag;
	std::vector<Target>	m_targets;
};

struct PMXRigidbody
{
	std::string	m_name;
	std::string	m_englishName;

	int32_t		m_boneIndex;
	uint8_t		m_group;
	uint16_t	m_collisionGroup;

	/*
	0:球
	1:箱
	2:カプセル
	*/
	enum class Shape : uint8_t
	{
		Sphere,
		Box,
		Capsule,
	};
	Shape		m_shape;
	glm::vec3	m_shapeSize;

	glm::vec3	m_translate;
	glm::vec3	m_rotate;	//ラジアン

	float	m_mass;
	float	m_translateDimmer;
	float	m_rotateDimmer;
	float	m_repulsion;
	float	m_friction;

	/*
	0:ボーン追従(static)
	1:物理演算(dynamic)
	2:物理演算 + Bone位置合わせ
	*/
	enum class Operation : uint8_t
	{
		Static,
		Dynamic,
		DynamicAndBoneMerge
	};
	Operation	m_op;
};

struct PMXJoint
{
	std::string	m_name;
	std::string	m_englishName;

	/*
	0:バネ付6DOF
	1:6DOF
	2:P2P
	3:ConeTwist
	4:Slider
	5:Hinge
	*/
	enum class JointType : uint8_t
	{
		SpringDOF6,
		DOF6,
		P2P,
		ConeTwist,
		Slider,
		Hinge,
	};
	JointType	m_type;
	int32_t		m_rigidbodyAIndex;
	int32_t		m_rigidbodyBIndex;

	glm::vec3	m_translate;
	glm::vec3	m_rotate;

	glm::vec3	m_translateLowerLimit;
	glm::vec3	m_translateUpperLimit;
	glm::vec3	m_rotateLowerLimit;
	glm::vec3	m_rotateUpperLimit;

	glm::vec3	m_springTranslateFactor;
	glm::vec3	m_springRotateFactor;
};

struct PMXSoftbody
{
	std::string	m_name;
	std::string	m_englishName;

	/*
	0:TriMesh
	1:Rope
	*/
	enum class SoftbodyType : uint8_t
	{
		TriMesh,
		Rope,
	};
	SoftbodyType	m_type;

	int32_t			m_materialIndex;

	uint8_t		m_group;
	uint16_t	m_collisionGroup;

	/*
	0x01:B-Link 作成
	0x02:クラスタ作成
	0x04: リンク交雑
	*/
	enum class SoftbodyMask : uint8_t
	{
		BLink = 0x01,
		Cluster = 0x02,
		HybridLink = 0x04,
	};
	SoftbodyMask	m_flag;

	int32_t	m_BLinkLength;
	int32_t	m_numClusters;

	float	m_totalMass;
	float	m_collisionMargin;

	/*
	1:V_TwoSided
	2:V_OneSided
	3:F_TwoSided
	4:F_OneSided
	*/
	enum class AeroModel : int32_t
	{
		kAeroModelV_TwoSided,
		kAeroModelV_OneSided,
		kAeroModelF_TwoSided,
		kAeroModelF_OneSided,
	};
	int32_t		m_aeroModel;

	//config
	float	m_VCF;
	float	m_DP;
	float	m_DG;
	float	m_LF;
	float	m_PR;
	float	m_VC;
	float	m_DF;
	float	m_MT;
	float	m_CHR;
	float	m_KHR;
	float	m_SHR;
	float	m_AHR;

	//cluster
	float	m_SRHR_CL;
	float	m_SKHR_CL;
	float	m_SSHR_CL;
	float	m_SR_SPLT_CL;
	float	m_SK_SPLT_CL;
	float	m_SS_SPLT_CL;

	// interaction
	int32_t	m_V_IT;
	int32_t	m_P_IT;
	int32_t	m_D_IT;
	int32_t	m_C_IT;

	//material
	float	m_LST;
	float	m_AST;
	float	m_VST;

	struct AnchorRigidbody
	{
		int32_t		m_rigidBodyIndex;
		int32_t		m_vertexIndex;
		uint8_t	m_nearMode; //0:FF 1:ON
	};
	std::vector<AnchorRigidbody> m_anchorRigidbodies;
	std::vector<int32_t> m_pinVertexIndices;
};

struct PMXFile
{
	PMXHeader	m_header;
	PMXInfo		m_info;

	std::vector<PMXVertex>		m_vertices;
	std::vector<PMXFace>		m_faces;
	std::vector<PMXTexture>		m_textures;
	std::vector<PMXMaterial>	m_materials;
	std::vector<PMXBone>		m_bones;
	std::vector<PMXMorph>		m_morphs;
	std::vector<PMXDispalyFrame>	m_displayFrames;
	std::vector<PMXRigidbody>	m_rigidbodies;
	std::vector<PMXJoint>		m_joints;
	std::vector<PMXSoftbody>	m_softbodies;
};

extern bool ReadPMXFile(PMXFile* pmxFile, const char* filename);

}}