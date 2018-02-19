//
// Copyright (C) Chaos Group & Golaem S.A. - All Rights Reserved.
//

#include "instance.h"
#include "vraygolaem.h"

#include <tomax.h>
#include <defparams.h>
#include <vray_plugins_ids.h>

using namespace VUtils;

static const PluginID GolaemMeshInstance_PluginID(LARGE_CONST(2016071298));
static Interval validForever(FOREVER);

VRayGolaemInstanceBase::VRayGolaemInstanceBase(VRayGolaem *vrayGolaem, INode *node, VRayCore *vray, int renderID)
	: vrayGolaem(vrayGolaem)
	, animatedTransform(NULL)
	, animatedFrameOffset(NULL)
{
	init(vrayGolaem, node, vray, renderID);

	// Set dummy mesh
	mesh = &dummyMesh;
}

static Transform getTransform(INode *inode, TimeValue t)
{
	vassert(inode);
	return toTransform(inode->GetObjectTM(t, &validForever) * maxToGolaem());
}

void VRayGolaemInstanceBase::frameBegin(TimeValue t, VRayCore *vray)
{
	vassert(vrayGolaem);

	VRenderInstance::frameBegin(t, vray);
	vrayGolaem->updateVRayParams(t);

	// Could be NULL if plugin DSO was not found.
	if (!animatedTransform || !animatedFrameOffset)
		return;

	TimeConversionRAII timeConversion(*vray);
	const double time = vray->getFrameData().t;

	VRaySettableParamInterface *settableTransform =
		queryInterface<VRaySettableParamInterface>(animatedTransform, EXT_SETTABLE_PARAM);
	vassert(settableTransform);

	VRaySettableParamInterface *settableFrameOffset =
		queryInterface<VRaySettableParamInterface>(animatedFrameOffset, EXT_SETTABLE_PARAM);
	vassert(settableFrameOffset);

	const Transform &tm = getTransform(node, t);
	settableTransform->setTransform(tm, 0, time);
	
	const float frameOffset = vrayGolaem->getCurrentFrameOffset(t);
	settableFrameOffset->setFloat(frameOffset, 0, time);
}

void VRayGolaemInstanceBase::newVRayPlugin(VRayCore &vray)
{
	VRenderPluginRendererInterface *pluginRenderer =
		queryInterface<VRenderPluginRendererInterface>(vray, EXT_VRENDER_PLUGIN_RENDERER);
	vassert(pluginRenderer);

	const TimeValue t = GetCOREInterface()->GetTime();

	GET_MBCS(node->GetName(), nodeName);

	// Correct the name of the shader to call. When exporting a scene from Maya with Vray,
	// some shader name special characters are replaced with not parsable character (":" => "__")
	// to be able to find the correct shader name to call, we need to apply the same conversion
	// to the shader names contained in the cam file
	CStr correctedCacheName(vrayGolaem->_cacheName);
	convertToValidVrsceneName(vrayGolaem->_cacheName, correctedCacheName);

	VRayPlugin *vrayGolaemPlugin =
		pluginRenderer->newPlugin(GolaemMeshInstance_PluginID, correctedCacheName.data());
	if (!vrayGolaemPlugin) {
		// XXX: Print some error.
		return;
	}

	VRayPlugin *nodePlugin =
		pluginRenderer->newPlugin(PluginId::Node, nodeName);
	vassert(nodePlugin);

	// XXX: Some dummy material may require.
	// nodePlugin->setParameter(pluginRenderer->newPluginParam("material", NULL));
	nodePlugin->setParameter(pluginRenderer->newBoolParam("visible", true));
	nodePlugin->setParameter(pluginRenderer->newTransformParam("transform", TraceTransform(1)));
	nodePlugin->setParameter(pluginRenderer->newPluginParam("geometry", vrayGolaemPlugin));

	// Transform could be updated in frameBegin().
	const Transform &tm = getTransform(node, t);
	animatedTransform = pluginRenderer->newTransformParam("proxyMatrix", TraceTransform(tm));
	vrayGolaemPlugin->setParameter(animatedTransform);

	const float frameOffset = vrayGolaem->getCurrentFrameOffset(t);
	animatedFrameOffset = pluginRenderer->newFloatParam("frameOffset", frameOffset);
	vrayGolaemPlugin->setParameter(animatedFrameOffset);

	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("cameraVisibility", vrayGolaem->_primaryVisibility));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("dccPackage", true));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("frustumCullingEnable", vrayGolaem->_frustumEnable));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("instancingEnable", vrayGolaem->_instancingEnable));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("layoutEnable", vrayGolaem->_layoutEnable));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("motionBlurEnable", vrayGolaem->_mBlurEnable));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("reflectionsVisibility", vrayGolaem->_visibleInReflections));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("refractionsVisibility", vrayGolaem->_visibleInRefractions));
	vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("shadowsVisibility", vrayGolaem->_castsShadows));

	vrayGolaemPlugin->setParameter(pluginRenderer->newIntParam("geometryTag", vrayGolaem->_geometryTag));
	vrayGolaemPlugin->setParameter(pluginRenderer->newIntParam("objectIdBase", vrayGolaem->_objectIDBase));
	vrayGolaemPlugin->setParameter(pluginRenderer->newIntParam("objectIdMode", vrayGolaem->_objectIDMode));

	vrayGolaemPlugin->setParameter(pluginRenderer->newFloatParam("cameraMargin", vrayGolaem->_cameraMargin));
	vrayGolaemPlugin->setParameter(pluginRenderer->newFloatParam("frustumMargin", vrayGolaem->_frustumMargin));
	vrayGolaemPlugin->setParameter(pluginRenderer->newFloatParam("renderPercent", vrayGolaem->_displayPercent));

	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("cacheFileDir", vrayGolaem->_cacheDir.data()));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("cacheName", vrayGolaem->_cacheName.data()));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("characterFiles", vrayGolaem->_characterFiles.data()));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("crowdField", vrayGolaem->_crowdFields.data()));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("defaultMaterial", vrayGolaem->_defaultMaterial.data()));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("layoutFile", vrayGolaem->_layoutFile.data()));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("proxyName", nodeName));
	vrayGolaemPlugin->setParameter(pluginRenderer->newStringParam("terrainFile", vrayGolaem->_terrainFile.data()));

	if (vrayGolaem->_overMBlurWindowSize) {
		vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("motionBlurWindowSize", vrayGolaem->_mBlurWindowSize));
	}
	if (vrayGolaem->_overMBlurSamples) {
		vrayGolaemPlugin->setParameter(pluginRenderer->newBoolParam("motionBlurSamples", vrayGolaem->_mBlurSamples));
	}
}
