#include "CommandList.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <algorithm>

CustomResources customResources;

void RunCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list,
		UINT VertexCount, UINT IndexCount, UINT InstanceCount)
{
	CommandList::iterator i;
	CommandListState state;
	ID3D11Device *mOrigDevice = mHackerDevice->GetOrigDevice();
	ID3D11DeviceContext *mOrigContext = mHackerContext->GetOrigContext();
	D3D11_MAPPED_SUBRESOURCE mappedResource;

	state.VertexCount = VertexCount;
	state.IndexCount = IndexCount;
	state.InstanceCount = InstanceCount;

	for (i = command_list->begin(); i < command_list->end(); i++) {
		(*i)->run(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &state);
	}

	if (state.update_params) {
		mOrigContext->Map(mHackerDevice->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
		mOrigContext->Unmap(mHackerDevice->mIniTexture, 0);
	}
}


static void ProcessParamRTSize(ID3D11DeviceContext *mOrigContext, CommandListState *state)
{
	D3D11_RENDER_TARGET_VIEW_DESC view_desc;
	D3D11_TEXTURE2D_DESC res_desc;
	ID3D11RenderTargetView *view = NULL;
	ID3D11Resource *res = NULL;
	ID3D11Texture2D *tex = NULL;

	if (state->rt_width != -1)
		return;

	mOrigContext->OMGetRenderTargets(1, &view, NULL);
	if (!view)
		return;

	view->GetDesc(&view_desc);

	if (view_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D &&
	    view_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS)
		goto out_release_view;

	view->GetResource(&res);
	if (!res)
		goto out_release_view;

	tex = (ID3D11Texture2D *)res;
	tex->GetDesc(&res_desc);

	state->rt_width = (float)res_desc.Width;
	state->rt_height = (float)res_desc.Height;

	tex->Release();
out_release_view:
	view->Release();
}

static float ProcessParamTextureFilter(HackerContext *mHackerContext,
		ID3D11DeviceContext *mOrigContext, ParamOverride *override)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ID3D11ShaderResourceView *view;
	ID3D11Resource *resource = NULL;
	TextureOverrideMap::iterator i;
	uint32_t hash = 0;
	float filter_index = 0;

	switch (override->shader_type) {
		case L'v':
			mOrigContext->VSGetShaderResources(override->texture_slot, 1, &view);
			break;
		case L'h':
			mOrigContext->HSGetShaderResources(override->texture_slot, 1, &view);
			break;
		case L'd':
			mOrigContext->DSGetShaderResources(override->texture_slot, 1, &view);
			break;
		case L'g':
			mOrigContext->GSGetShaderResources(override->texture_slot, 1, &view);
			break;
		case L'p':
			mOrigContext->PSGetShaderResources(override->texture_slot, 1, &view);
			break;
		case L'c':
			mOrigContext->CSGetShaderResources(override->texture_slot, 1, &view);
			break;
		default:
			// Should not happen
			return filter_index;
	}
	if (!view)
		return filter_index;


	view->GetResource(&resource);
	if (!resource)
		goto out_release_view;

	view->GetDesc(&desc);

	switch (desc.ViewDimension) {
		case D3D11_SRV_DIMENSION_TEXTURE2D:
		case D3D11_SRV_DIMENSION_TEXTURE2DMS:
		case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
			hash = mHackerContext->GetTexture2DHash((ID3D11Texture2D *)resource, false, NULL);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE3D:
			hash = mHackerContext->GetTexture3DHash((ID3D11Texture3D *)resource, false, NULL);
			break;
	}
	if (!hash)
		goto out_release_resource;

	i = G->mTextureOverrideMap.find(hash);
	if (i == G->mTextureOverrideMap.end())
		goto out_release_resource;

	filter_index = i->second.filter_index;

out_release_resource:
	resource->Release();
out_release_view:
	view->Release();
	return filter_index;
}

void ParamOverride::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	float *dest = &(G->iniParams[param_idx].*param_component);

	float orig = *dest;

	switch (type) {
		case ParamOverrideType::VALUE:
			*dest = val;
			break;
		case ParamOverrideType::RT_WIDTH:
			ProcessParamRTSize(mOrigContext, state);
			*dest = state->rt_width;
			break;
		case ParamOverrideType::RT_HEIGHT:
			ProcessParamRTSize(mOrigContext, state);
			*dest = state->rt_height;
			break;
		case ParamOverrideType::RES_WIDTH:
			*dest = (float)G->mResolutionInfo.width;
			break;
		case ParamOverrideType::RES_HEIGHT:
			*dest = (float)G->mResolutionInfo.height;
			break;
		case ParamOverrideType::TEXTURE:
			*dest = ProcessParamTextureFilter(mHackerContext,
					mOrigContext, this);
			break;
		case ParamOverrideType::VERTEX_COUNT:
			*dest = (float)state->VertexCount;
			break;
		case ParamOverrideType::INDEX_COUNT:
			*dest = (float)state->IndexCount;
			break;
		case ParamOverrideType::INSTANCE_COUNT:
			*dest = (float)state->InstanceCount;
			break;
		default:
			return;
	}
	state->update_params |= (*dest != orig);
}

// Parse IniParams overrides, in forms such as
// x = 0.3 (set parameter to specific value, e.g. for shader partner filtering)
// y2 = ps-t0 (use parameter for texture filtering based on texture slot of shader type)
// z3 = rt_width / rt_height (set parameter to render target width/height)
// w4 = res_width / res_height (set parameter to resolution width/height)
bool ParseCommandListIniParamOverride(const wchar_t *key, wstring *val,
		CommandList *command_list)
{
	int ret, len1, len2;
	size_t length = wcslen(key);
	wchar_t component;
	ParamOverride *param = new ParamOverride();

	// Parse key
	ret = swscanf_s(key, L"%lc%n%u%n", &component, 1, &len1, &param->param_idx, &len2);

	// May or may not have matched index. Make sure entire string was
	// matched either way and check index is valid if it was matched:
	if (ret == 1 && len1 == length) {
		param->param_idx = 0;
	} else if (ret == 2 && len2 == length) {
		if (param->param_idx >= INI_PARAMS_SIZE)
			goto bail;
	} else {
		goto bail;
	}

	switch (component) {
		case L'x':
			param->param_component = &DirectX::XMFLOAT4::x;
			break;
		case L'y':
			param->param_component = &DirectX::XMFLOAT4::y;
			break;
		case L'z':
			param->param_component = &DirectX::XMFLOAT4::z;
			break;
		case L'w':
			param->param_component = &DirectX::XMFLOAT4::w;
			break;
		default:
			goto bail;
	}

	// Try parsing value as a float
	ret = swscanf_s(val->c_str(), L"%f%n", &param->val, &len1);
	if (ret != 0 && ret != EOF && len1 == val->length()) {
		param->type = ParamOverrideType::VALUE;
		goto success;
	}

	// Try parsing value as "<shader type>s-t<testure slot>" for texture filtering
	ret = swscanf_s(val->c_str(), L"%lcs-t%u%n", &param->shader_type, 1, &param->texture_slot, &len1);
	if (ret == 2 && len1 == val->length() &&
			param->texture_slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		switch(param->shader_type) {
			case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
				param->type = ParamOverrideType::TEXTURE;
				goto success;
			default:
				goto bail;
		}
	}

	// Check special keywords
	param->type = lookup_enum_val<const wchar_t *, ParamOverrideType>
		(ParamOverrideTypeNames, val->c_str(), ParamOverrideType::INVALID);
	if (param->type == ParamOverrideType::INVALID)
		goto bail;

success:
	command_list->push_back(std::unique_ptr<CommandListCommand>(param));
	return true;
bail:
	delete param;
	return false;
}


CustomResource::CustomResource() :
	resource(NULL),
	view(NULL),
	is_null(true),
	substantiated(false),
	bind_flags((D3D11_BIND_FLAG)0),
	stride(0),
	offset(0),
	format(DXGI_FORMAT_UNKNOWN),
	max_copies_per_frame(0),
	frame_no(0),
	copies_this_frame(0)
{}

CustomResource::~CustomResource()
{
	if (resource)
		resource->Release();
	if (view)
		view->Release();
}

void CustomResource::Substantiate(ID3D11Device *mOrigDevice)
{
	wstring ext;
	HRESULT hr;

	// We only allow a custom resource to be substantiated once. Otherwise
	// we could end up reloading it again if it is later set to null. Also
	// prevents us from endlessly retrying to load a custom resource from a
	// file that doesn't exist:
	if (substantiated)
		return;
	substantiated = true;

	// If this custom resource has already been set through other means we
	// won't overwrite it:
	if (resource || view)
		return;

	// If the resource section has enough information to create a resource
	// we do so the first time it is loaded from. The reason we do it this
	// late is to make sure we know which device is actually being used to
	// render the game - FC4 creates about a dozen devices with different
	// parameters while probing the hardware before it settles on the one
	// it will actually use.

	// TODO: Support loading raw buffers (may want more ini params to
	// describe them)

	if (!filename.empty()) {
		// XXX: We are not creating a view with DirecXTK because
		// 1) it assumes we want a shader resource view, which is an
		//    assumption that doesn't fit with the goal of this code to
		//    allow for arbitrary resource copying, and
		// 2) we currently won't use the view in a source custom
		//    resource, even if we are referencing it into a compatible
		//    slot. We might improve this, and if we do, I don't want
		//    any surprises caused by a view of the wrong type we
		//    happen to have created here and forgotten about.
		// If we do start using the source custom resource's view, we
		// could do something smart here, like only using it if the
		// bind_flags indicate it will be used as a shader resource.

		ext = filename.substr(filename.rfind(L"."));
		if (!_wcsicmp(ext.c_str(), L".dds")) {
			LogInfoW(L"Loading custom resource %s as DDS\n", filename.c_str());
			hr = DirectX::CreateDDSTextureFromFileEx(mOrigDevice,
					filename.c_str(), 0,
					D3D11_USAGE_DEFAULT, bind_flags, 0, 0,
					false, &resource, NULL, NULL);
		} else {
			LogInfoW(L"Loading custom resource %s as WIC\n", filename.c_str());
			hr = DirectX::CreateWICTextureFromFileEx(mOrigDevice,
					filename.c_str(), 0,
					D3D11_USAGE_DEFAULT, bind_flags, 0, 0,
					false, &resource, NULL);
		}
		if (SUCCEEDED(hr)) {
			is_null = false;
			// TODO:
			// format = ...
		} else
			LogInfoW(L"Failed to load custom resource %s: 0x%x\n", filename.c_str(), hr);
	}
}


bool ResourceCopyTarget::ParseTarget(const wchar_t *target, bool allow_null)
{
	int ret, len;
	size_t length = wcslen(target);
	CustomResources::iterator res;

	ret = swscanf_s(target, L"%lcs-cb%u%n", &shader_type, 1, &slot, &len);
	if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
		type = ResourceCopyTargetType::CONSTANT_BUFFER;
		goto check_shader_type;
	}

	ret = swscanf_s(target, L"%lcs-t%u%n", &shader_type, 1, &slot, &len);
	if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		type = ResourceCopyTargetType::SHADER_RESOURCE;
	       goto check_shader_type;
	}

	// TODO: ret = swscanf_s(target, L"%lcs-s%u%n", &shader_type, 1, &slot, &len);
	// TODO: if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
	// TODO: 	type = ResourceCopyTargetType::SAMPLER;
	// TODO:	goto check_shader_type;
	// TODO: }

	ret = swscanf_s(target, L"o%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT) {
		type = ResourceCopyTargetType::RENDER_TARGET;
		return true;
	}

	if (!wcscmp(target, L"oD")) {
		type = ResourceCopyTargetType::DEPTH_STENCIL_TARGET;
		return true;
	}

	ret = swscanf_s(target, L"%lcs-u%u%n", &shader_type, 1, &slot, &len);
	// XXX: On Win8 D3D11_1_UAV_SLOT_COUNT (64) is the limit instead. Use
	// the lower amount for now to enforce compatibility.
	if (ret == 2 && len == length && slot < D3D11_PS_CS_UAV_REGISTER_COUNT) {
		// These views are only valid for pixel and compute shaders:
		if (shader_type == L'p' || shader_type == L'c') {
			type = ResourceCopyTargetType::UNORDERED_ACCESS_VIEW;
			return true;
		}
		return false;
	}

	ret = swscanf_s(target, L"vb%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) {
		type = ResourceCopyTargetType::VERTEX_BUFFER;
		return true;
	}

	if (!wcscmp(target, L"ib")) {
		type = ResourceCopyTargetType::INDEX_BUFFER;
		return true;
	}

	ret = swscanf_s(target, L"so%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D11_SO_STREAM_COUNT) {
		type = ResourceCopyTargetType::STREAM_OUTPUT;
		return true;
	}

	if (allow_null && !wcscmp(target, L"null")) {
		type = ResourceCopyTargetType::EMPTY;
		return true;
	}

	if (length >= 9 && !_wcsnicmp(target, L"resource", 8)) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		wstring resource_id(target);
		std::transform(resource_id.begin(), resource_id.end(), resource_id.begin(), ::towlower);

		res = customResources.find(resource_id);
		if (res == customResources.end())
			return false;

		custom_resource = &res->second;
		type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		return true;
	}

	return false;

check_shader_type:
	switch(shader_type) {
		case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
			return true;
	}
	return false;
}


bool ParseCommandListResourceCopyDirective(const wchar_t *key, wstring *val,
		CommandList *command_list)
{
	ResourceCopyOperation *operation = new ResourceCopyOperation();
	wchar_t buf[MAX_PATH];
	wchar_t *src_ptr = NULL;

	if (!operation->dst.ParseTarget(key, false))
		goto bail;

	// parse_enum_option_string replaces spaces with NULLs, so it can't
	// operate on the buffer in the wstring directly. I could potentially
	// change it to work without modifying the string, but for now it's
	// easier to just make a copy of the string:
	if (val->length() >= MAX_PATH)
		goto bail;
	val->copy(buf, MAX_PATH, 0);
	buf[val->length()] = L'\0';

	operation->options = parse_enum_option_string<wchar_t *, ResourceCopyOptions>
		(ResourceCopyOptionNames, buf, &src_ptr);

	if (!src_ptr)
		goto bail;

	if (!operation->src.ParseTarget(src_ptr, true))
		goto bail;

	if (!(operation->options & ResourceCopyOptions::COPY_TYPE_MASK)) {
		// If the copy method was not speficied make a guess.
		// References aren't always safe (e.g. a resource can't be both
		// an input and an output), and a resource may not have been
		// created with the right usage flags, so we'll err on the side
		// of doing a full copy if we aren't fairly sure.
		//
		// If we're merely copying a resource from one shader to
		// another without changnig the usage (e.g. giving the vertex
		// shader access to a constant buffer or texture from the pixel
		// shader) a reference is probably safe (unless the game
		// reassigns it to a different usage later and doesn't know
		// that our reference is still bound somewhere), but it would
		// not be safe to give a vertex shader access to the depth
		// buffer of the output merger stage, for example.
		//
		// If we are copying a resource into a custom resource (e.g.
		// for use from another draw call), do a full copy by default
		// in case the game alters the original.
		if (operation->dst.type == ResourceCopyTargetType::CUSTOM_RESOURCE)
			operation->options |= ResourceCopyOptions::COPY;
		else if (operation->src.type == ResourceCopyTargetType::CUSTOM_RESOURCE)
			operation->options |= ResourceCopyOptions::REFERENCE;
		else if (operation->src.type == operation->dst.type)
			operation->options |= ResourceCopyOptions::REFERENCE;
		else
			operation->options |= ResourceCopyOptions::COPY;
	}

	// FIXME: If custom resources are copied to other custom resources by
	// reference that are in turn bound to the pipeline we may not
	// propagate all the bind flags correctly depending on the order
	// everything is parsed. We'd need to construct a dependency graph
	// to fix this, but it's not clear that this combination would really
	// be used in practice, so for now this will do.
	// FIXME: The constant buffer bind flag can't be combined with others
	if (operation->src.type == ResourceCopyTargetType::CUSTOM_RESOURCE &&
			(operation->options & ResourceCopyOptions::REFERENCE)) {
		// Fucking C++ making this line 3x longer than it should be:
		operation->src.custom_resource->bind_flags = (D3D11_BIND_FLAG)
			(operation->src.custom_resource->bind_flags | operation->dst.BindFlags());
	}

	command_list->push_back(std::unique_ptr<CommandListCommand>(operation));
	return true;
bail:
	delete operation;
	return false;
}

ID3D11Resource *ResourceCopyTarget::GetResource(
		ID3D11Device *mOrigDevice,
		ID3D11DeviceContext *mOrigContext,
		ID3D11View **view,   // Used by textures, render targets, depth/stencil buffers & UAVs
		UINT *stride,        // Used by vertex buffers
		UINT *offset,        // Used by vertex & index buffers
		DXGI_FORMAT *format) // Used by index buffers
{
	ID3D11Resource *res = NULL;
	ID3D11Buffer *buf = NULL;
	ID3D11Buffer *so_bufs[D3D11_SO_STREAM_COUNT];
	ID3D11ShaderResourceView *resource_view = NULL;
	ID3D11RenderTargetView *render_view[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *depth_view = NULL;
	ID3D11UnorderedAccessView *unordered_view = NULL;
	unsigned i;

	switch(type) {
	case ResourceCopyTargetType::CONSTANT_BUFFER:
		// FIXME: On win8 (or with evil update?), we should use
		// Get/SetConstantBuffers1 and copy the offset into the buffer as well
		switch(shader_type) {
		case L'v':
			mOrigContext->VSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'h':
			mOrigContext->HSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'd':
			mOrigContext->DSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'g':
			mOrigContext->GSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'p':
			mOrigContext->PSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'c':
			mOrigContext->CSGetConstantBuffers(slot, 1, &buf);
			return buf;
		default:
			// Should not happen
			return NULL;
		}
		break;

	case ResourceCopyTargetType::SHADER_RESOURCE:
		switch(shader_type) {
		case L'v':
			mOrigContext->VSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'h':
			mOrigContext->HSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'd':
			mOrigContext->DSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'g':
			mOrigContext->GSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'p':
			mOrigContext->PSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'c':
			mOrigContext->CSGetShaderResources(slot, 1, &resource_view);
			break;
		default:
			// Should not happen
			return NULL;
		}

		if (!resource_view)
			return NULL;

		resource_view->GetResource(&res);
		if (!res) {
			resource_view->Release();
			return NULL;
		}

		// TODO: For buffer type resources, get the stride, offset and
		// format from the description

		*view = resource_view;
		return res;

	// TODO: case ResourceCopyTargetType::SAMPLER: // Not an ID3D11Resource, need to think about this one
	// TODO: 	break;

	case ResourceCopyTargetType::VERTEX_BUFFER:
		// TODO: If copying this to a constant buffer, provide some
		// means to get the strides + offsets from within the shader.
		// Perhaps as an IniParam, or in another constant buffer?
		mOrigContext->IAGetVertexBuffers(slot, 1, &buf, stride, offset);
		return buf;

	case ResourceCopyTargetType::INDEX_BUFFER:
		// TODO: Similar comment as vertex buffers above, provide a
		// means for a shader to get format + offset.
		mOrigContext->IAGetIndexBuffer(&buf, format, offset);
		return buf;

	case ResourceCopyTargetType::STREAM_OUTPUT:
		// XXX: Does not give us the offset
		mOrigContext->SOGetTargets(slot + 1, so_bufs);

		// Release any buffers we aren't after:
		for (i = 0; i < slot; i++) {
			if (so_bufs[i]) {
				so_bufs[i]->Release();
				so_bufs[i] = NULL;
			}
		}

		return so_bufs[slot];

	case ResourceCopyTargetType::RENDER_TARGET:
		mOrigContext->OMGetRenderTargets(slot + 1, render_view, NULL);

		// Release any views we aren't after:
		for (i = 0; i < slot; i++) {
			if (render_view[i]) {
				render_view[i]->Release();
				render_view[i] = NULL;
			}
		}

		if (!render_view[slot])
			return NULL;

		render_view[slot]->GetResource(&res);
		if (!res) {
			render_view[slot]->Release();
			return NULL;
		}

		// TODO: For buffer type resources, get the stride, offset and
		// format from the description

		*view = render_view[slot];
		return res;

	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		mOrigContext->OMGetRenderTargets(0, NULL, &depth_view);
		if (!depth_view)
			return NULL;

		depth_view->GetResource(&res);
		if (!res) {
			depth_view->Release();
			return NULL;
		}

		// TODO: For buffer type resources, get the stride, offset and
		// format from the description

		*view = depth_view;
		return res;

	case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
		switch(shader_type) {
		case L'p':
			// XXX: Not clear if the start slot is ok like this from the docs?
			// Particularly, what happens if we retrieve a subsequent UAV?
			mOrigContext->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, slot, 1, &unordered_view);
			break;
		case L'c':
			mOrigContext->CSGetUnorderedAccessViews(slot, 1, &unordered_view);
			break;
		default:
			// Should not happen
			return NULL;
		}

		if (!unordered_view)
			return NULL;

		unordered_view->GetResource(&res);
		if (!res) {
			unordered_view->Release();
			return NULL;
		}

		// TODO: For buffer type resources, get the stride, offset and
		// format from the description

		*view = unordered_view;
		return res;

	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		custom_resource->Substantiate(mOrigDevice);

		*stride = custom_resource->stride;
		*offset = custom_resource->offset;
		*format = custom_resource->format;

		if (custom_resource->is_null) {
			// Optimisation to allow the resource to be set to null
			// without throwing away the cache so we don't
			// endlessly create & destroy temporary resources.
			*view = NULL;
			return NULL;
		}

		if (custom_resource->view)
			custom_resource->view->AddRef();
		*view = custom_resource->view;
		if (custom_resource->resource)
			custom_resource->resource->AddRef();
		return custom_resource->resource;
	}

	return NULL;
}

void ResourceCopyTarget::SetResource(
		ID3D11DeviceContext *mOrigContext,
		ID3D11Resource *res,
		ID3D11View *view,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format)
{
	ID3D11Buffer *buf = NULL;
	ID3D11Buffer *so_bufs[D3D11_SO_STREAM_COUNT];
	ID3D11ShaderResourceView *resource_view = NULL;
	ID3D11RenderTargetView *render_view[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *depth_view = NULL;
	ID3D11UnorderedAccessView *unordered_view = NULL;
	UINT uav_counter = -1; // TODO: Allow this to be set
	int i;

	switch(type) {
	case ResourceCopyTargetType::CONSTANT_BUFFER:
		// FIXME: On win8 (or with evil update?), we should use
		// Get/SetConstantBuffers1 and copy the offset into the buffer as well
		buf = (ID3D11Buffer*)res;
		switch(shader_type) {
		case L'v':
			mOrigContext->VSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'h':
			mOrigContext->HSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'd':
			mOrigContext->DSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'g':
			mOrigContext->GSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'p':
			mOrigContext->PSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'c':
			mOrigContext->CSSetConstantBuffers(slot, 1, &buf);
			return;
		default:
			// Should not happen
			return;
		}
		break;

	case ResourceCopyTargetType::SHADER_RESOURCE:
		resource_view = (ID3D11ShaderResourceView*)view;
		switch(shader_type) {
		case L'v':
			mOrigContext->VSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'h':
			mOrigContext->HSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'd':
			mOrigContext->DSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'g':
			mOrigContext->GSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'p':
			mOrigContext->PSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'c':
			mOrigContext->CSSetShaderResources(slot, 1, &resource_view);
			break;
		default:
			// Should not happen
			return;
		}
		break;

	// TODO: case ResourceCopyTargetType::SAMPLER: // Not an ID3D11Resource, need to think about this one
	// TODO: 	break;

	case ResourceCopyTargetType::VERTEX_BUFFER:
		buf = (ID3D11Buffer*)res;
		mOrigContext->IASetVertexBuffers(slot, 1, &buf, &stride, &offset);
		return;

	case ResourceCopyTargetType::INDEX_BUFFER:
		buf = (ID3D11Buffer*)res;
		mOrigContext->IASetIndexBuffer(buf, format, offset);
		break;

	case ResourceCopyTargetType::STREAM_OUTPUT:
		// XXX: HERE BE UNTESTED CODE PATHS!
		buf = (ID3D11Buffer*)res;
		mOrigContext->SOGetTargets(D3D11_SO_STREAM_COUNT, so_bufs);
		if (so_bufs[slot])
			so_bufs[slot]->Release();
		so_bufs[slot] = buf;
		// XXX: We set offsets to NULL here. We should really preserve
		// them, but I'm not sure how to get their original values,
		// so... too bad. Probably will never even use this anyway.
		mOrigContext->SOSetTargets(D3D11_SO_STREAM_COUNT, so_bufs, NULL);

		for (i = 0; i < D3D11_SO_STREAM_COUNT; i++) {
			if (so_bufs[i])
				so_bufs[i]->Release();
		}

		break;

	case ResourceCopyTargetType::RENDER_TARGET:
	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		// XXX: HERE BE UNTESTED CODE PATHS!
		mOrigContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);
		if (type == ResourceCopyTargetType::RENDER_TARGET) {
			if (render_view[slot])
				render_view[slot]->Release();
			render_view[slot] = (ID3D11RenderTargetView*)view;
		} else {
			if (depth_view)
				depth_view->Release();
			depth_view = (ID3D11DepthStencilView*)view;
		}
		mOrigContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

		for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
			if (render_view[i])
				render_view[i]->Release();
		}
		depth_view->Release();
		break;

	case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
		// XXX: HERE BE UNTESTED CODE PATHS!
		unordered_view = (ID3D11UnorderedAccessView*)view;
		switch(shader_type) {
		case L'p':
			// XXX: Not clear if this will unbind other UAVs or not?
			// TODO: Allow pUAVInitialCounts to optionally be set
			mOrigContext->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
				NULL, NULL, slot, 1, &unordered_view, &uav_counter);
			return;
		case L'c':
			// TODO: Allow pUAVInitialCounts to optionally be set
			mOrigContext->CSSetUnorderedAccessViews(slot, 1, &unordered_view, &uav_counter);
			return;
		default:
			// Should not happen
			return;
		}
		break;

	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		custom_resource->stride = stride;
		custom_resource->offset = offset;
		custom_resource->format = format;


		if (res == NULL && view == NULL) {
			// Optimisation to allow the resource to be set to null
			// without throwing away the cache so we don't
			// endlessly create & destroy temporary resources.
			custom_resource->is_null = true;
			return;
		}
		custom_resource->is_null = false;

		// If we are passed our own resource (might happen if the
		// resource is used directly in the run() function, or if
		// someone assigned a resource to itself), don't needlessly
		// AddRef() and Release(), and definitely don't Release()
		// before AddRef()
		if (custom_resource->view != view) {
			if (custom_resource->view)
				custom_resource->view->Release();
			custom_resource->view = view;
			if (custom_resource->view)
				custom_resource->view->AddRef();
		}

		if (custom_resource->resource != res) {
			if (custom_resource->resource)
				custom_resource->resource->Release();
			custom_resource->resource = res;
			if (custom_resource->resource)
				custom_resource->resource->AddRef();
		}
		break;
	}
}

D3D11_BIND_FLAG ResourceCopyTarget::BindFlags()
{
	switch(type) {
		case ResourceCopyTargetType::CONSTANT_BUFFER:
			return D3D11_BIND_CONSTANT_BUFFER;
		case ResourceCopyTargetType::SHADER_RESOURCE:
			return D3D11_BIND_SHADER_RESOURCE;
		case ResourceCopyTargetType::VERTEX_BUFFER:
			return D3D11_BIND_VERTEX_BUFFER;
		case ResourceCopyTargetType::INDEX_BUFFER:
			return D3D11_BIND_INDEX_BUFFER;
		case ResourceCopyTargetType::STREAM_OUTPUT:
			return D3D11_BIND_STREAM_OUTPUT;
		case ResourceCopyTargetType::RENDER_TARGET:
			return D3D11_BIND_RENDER_TARGET;
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			return D3D11_BIND_DEPTH_STENCIL;
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			return D3D11_BIND_UNORDERED_ACCESS;
		case ResourceCopyTargetType::CUSTOM_RESOURCE:
			return custom_resource->bind_flags;
	}

	// Shouldn't happen. No return value makes sense, so raise an exception
	throw(std::range_error("Bad 3DMigoto ResourceCopyTarget"));
}

static bool IsCoersionToStructuredBufferRequired(ID3D11View *view, UINT stride,
		UINT offset, DXGI_FORMAT format, D3D11_BIND_FLAG bind_flags)
{
	// If we are copying a vertex buffer into a shader resource we need to
	// convert it into a structured buffer, which requires a flag set when
	// creating the new resource as well as changes in the view.
	//
	// This function tries to detect this situation without explicitly
	// checking that the source was a vertex buffer - that way, similar
	// situations should work as well, such as when using an intermediate
	// resource.

	// If we are copying from a resource that had a view we will use it's
	// description to work out what we need to do (or we will, once I write
	// that code)
	if (view)
		return false;

	// If we know the format there's no need to be structured
	if (format != DXGI_FORMAT_UNKNOWN)
		return false;

	// We need to know the stride to be structured:
	if (stride == 0)
		return false;

	// Structured buffers only make sense for certain views:
	return !!(bind_flags & (D3D11_BIND_SHADER_RESOURCE |
			D3D11_BIND_RENDER_TARGET |
			D3D11_BIND_DEPTH_STENCIL |
			D3D11_BIND_UNORDERED_ACCESS));
}

static ID3D11Buffer *RecreateCompatibleBuffer(
		ID3D11Buffer *src_resource,
		ID3D11Buffer *dst_resource,
		ID3D11View *src_view,
		D3D11_BIND_FLAG bind_flags,
		ID3D11Device *device,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT *buf_src_size,
		UINT *buf_dst_size)
{
	HRESULT hr;
	D3D11_BUFFER_DESC old_desc;
	D3D11_BUFFER_DESC new_desc;
	ID3D11Buffer *buffer = NULL;
	UINT dst_size;

	src_resource->GetDesc(&new_desc);
	*buf_src_size = new_desc.ByteWidth;
	new_desc.Usage = D3D11_USAGE_DEFAULT;
	new_desc.BindFlags = bind_flags;
	new_desc.CPUAccessFlags = 0;

	// TODO: Add a keyword to allow raw views:
	// D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS

	if (bind_flags & D3D11_BIND_CONSTANT_BUFFER) {
		// Constant buffers have additional limitations. The size must
		// be a multiple of 16, so round up if necessary, and it cannot
		// be larger than 4096 x 4 component x 4 byte constants.
		dst_size = (new_desc.ByteWidth + 15) & ~0xf;
		dst_size = min(dst_size, D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);

		// If the size of the new resource doesn't match the old or
		// there is an offset we will have to perform a region copy
		// instead of a regular copy:
		if (offset || dst_size != new_desc.ByteWidth) {
			// It might be temping to take the offset into account
			// here and make the buffer only as large as it need to
			// be, but it's possible that the source offset might
			// change much more often than the source buffer (just
			// a guess), which could potentially lead us to
			// constantly recreating the destination buffer.

			// Note down the size of the source and destination:
			*buf_dst_size = dst_size;
			new_desc.ByteWidth = dst_size;
		}
	} else if (IsCoersionToStructuredBufferRequired(src_view, stride, offset, format, bind_flags)) {
		new_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		new_desc.StructureByteStride = stride;

		// A structured buffer needs to be a multiple of it's stride,
		// which may not be the case if we're converting a buffer to
		// one. Round it down:
		dst_size = new_desc.ByteWidth / stride * stride;
		// For now always using the region copy if there's an offset.
		// We might not need to do that if the offset is aligned to the
		// stride (although we would need to recreate the view every
		// time it changed), but for now it seems safest to use the
		// region copy method whenever there is an offset:
		if (offset || dst_size != new_desc.ByteWidth) {
			*buf_dst_size = dst_size;
			new_desc.ByteWidth = dst_size;
		}
	} else if (!src_view && offset) {
		// No source view but we do have an offset - use the region
		// copy to knock out the offset. We can probably assume the
		// original resource met all the size and alignment
		// constraints, so we shouldn't need to resize it.
		*buf_dst_size = new_desc.ByteWidth;
	}

	if (dst_resource) {
		// If destination already exists and the description is
		// identical we don't need to recreate it.
		dst_resource->GetDesc(&old_desc);
		if (!memcmp(&old_desc, &new_desc, sizeof(D3D11_BUFFER_DESC)))
			return NULL;
		LogInfo("RecreateCompatibleBuffer: Recreating cached resource\n");
	} else
		LogInfo("RecreateCompatibleBuffer: Creating cached resource\n");

	LogDebug("  ByteWidth = %d\n", new_desc.ByteWidth);
	LogDebug("  Usage = %d\n", new_desc.Usage);
	LogDebug("  BindFlags = %x\n", new_desc.BindFlags);
	LogDebug("  CPUAccessFlags = %x\n", new_desc.CPUAccessFlags);
	LogDebug("  MiscFlags = %x\n", new_desc.MiscFlags);
	LogDebug("  StructureByteStride = %d\n", new_desc.StructureByteStride);

	hr = device->CreateBuffer(&new_desc, NULL, &buffer);
	if (FAILED(hr)) {
		LogInfo("Resource copy RecreateCompatibleBuffer failed: 0x%x\n", hr);
		return NULL;
	}

	return buffer;
}

// MSAA resolving only makes sense for Texture2D types, and the SampleDesc
// entry only exists in those. Use template specialisation so we don't have to
// duplicate the entire RecreateCompatibleTexture() routine for such a small
// difference.
template <typename DescType>
static void Texture2DDescResolveMSAA(DescType *desc) {}
template <>
static void Texture2DDescResolveMSAA(D3D11_TEXTURE2D_DESC *desc)
{
	desc->SampleDesc.Count = 1;
	desc->SampleDesc.Quality = 0;
}

template <typename ResourceType,
	 typename DescType,
	HRESULT (__stdcall ID3D11Device::*CreateTexture)(THIS_
	      const DescType *pDesc,
	      const D3D11_SUBRESOURCE_DATA *pInitialData,
	      ResourceType **ppTexture)
	>
static ResourceType* RecreateCompatibleTexture(
		ResourceType *src_resource,
		ResourceType *dst_resource,
		D3D11_BIND_FLAG bind_flags,
		DXGI_FORMAT format,
		ID3D11Device *device,
		StereoHandle mStereoHandle,
		ResourceCopyOptions options)
{
	HRESULT hr;
	DescType old_desc;
	DescType new_desc;
	ResourceType *tex = NULL;

	src_resource->GetDesc(&new_desc);
	new_desc.Usage = D3D11_USAGE_DEFAULT;
	new_desc.BindFlags = bind_flags;
	new_desc.CPUAccessFlags = 0;
#if 0
	// Didn't seem to work - got an invalid argument error from
	// CreateTexture2D. Could be that the existing view used a format
	// incompatible with the bind flags (e.g. depth+stencil formats can't
	// be used in a shader resource).
	if (format)
		new_desc.Format = format;
#else
	new_desc.Format = EnsureNotTypeless(new_desc.Format);
#endif

	if (options & ResourceCopyOptions::STEREO2MONO)
		new_desc.Width *= 2;

	// TODO: reverse_blit might need to imply resolve_msaa:
	if (options & ResourceCopyOptions::RESOLVE_MSAA)
		Texture2DDescResolveMSAA(&new_desc);

	// XXX: Any changes needed in new_desc.MiscFlags?

	if (dst_resource) {
		// If destination already exists and the description is
		// identical we don't need to recreate it.
		dst_resource->GetDesc(&old_desc);
		if (!memcmp(&old_desc, &new_desc, sizeof(DescType)))
			return NULL;
		LogInfo("RecreateCompatibleTexture: Recreating cached resource\n");
	} else
		LogInfo("RecreateCompatibleTexture: Creating cached resource\n");

	hr = (device->*CreateTexture)(&new_desc, NULL, &tex);
	if (FAILED(hr)) {
		LogInfo("Resource copy RecreateCompatibleTexture failed: 0x%x\n", hr);
		return NULL;
	}

	return tex;
}

static DXGI_FORMAT GetViewFormat(ResourceCopyTargetType type, ID3D11View *view)
{
	ID3D11ShaderResourceView *resource_view = NULL;
	ID3D11RenderTargetView *render_view = NULL;
	ID3D11DepthStencilView *depth_view = NULL;
	ID3D11UnorderedAccessView *unordered_view = NULL;

	D3D11_SHADER_RESOURCE_VIEW_DESC resource_view_desc;
	D3D11_RENDER_TARGET_VIEW_DESC render_view_desc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc;
	D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_view_desc;

	if (!view)
		return DXGI_FORMAT_UNKNOWN;

	switch (type) {
		case ResourceCopyTargetType::SHADER_RESOURCE:
			resource_view = (ID3D11ShaderResourceView*)view;
			resource_view->GetDesc(&resource_view_desc);
			return resource_view_desc.Format;
		case ResourceCopyTargetType::RENDER_TARGET:
			render_view = (ID3D11RenderTargetView*)view;
			render_view->GetDesc(&render_view_desc);
			return render_view_desc.Format;
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			depth_view = (ID3D11DepthStencilView*)view;
			depth_view->GetDesc(&depth_view_desc);
			return depth_view_desc.Format;
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			unordered_view = (ID3D11UnorderedAccessView*)view;
			unordered_view->GetDesc(&unordered_view_desc);
			return unordered_view_desc.Format;
	}

	return DXGI_FORMAT_UNKNOWN;
}

static void RecreateCompatibleResource(
		ResourceCopyTarget *dst,
		ID3D11Resource *src_resource,
		ID3D11Resource **dst_resource,
		ID3D11View *src_view,
		ID3D11View **dst_view,
		ID3D11Device *device,
		StereoHandle mStereoHandle,
		ResourceCopyOptions options,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT *buf_src_size,
		UINT *buf_dst_size)
{
	NVAPI_STEREO_SURFACECREATEMODE orig_mode;
	D3D11_RESOURCE_DIMENSION src_dimension;
	D3D11_RESOURCE_DIMENSION dst_dimension;
	D3D11_BIND_FLAG bind_flags = (D3D11_BIND_FLAG)0;
	ID3D11Resource *res = NULL;

	if (dst)
		bind_flags = dst->BindFlags();

	src_resource->GetType(&src_dimension);
	if (*dst_resource) {
		(*dst_resource)->GetType(&dst_dimension);
		if (src_dimension != dst_dimension) {
			LogInfo("RecreateCompatibleResource: Resource type changed\n");

			(*dst_resource)->Release();
			if (dst_view && *dst_view)
				(*dst_view)->Release();

			*dst_resource = NULL;
			if (dst_view)
				*dst_view = NULL;
		}
	}

	if (options & ResourceCopyOptions::CREATEMODE_MASK) {
		NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, &orig_mode);

		// STEREO2MONO will force the final destination to mono since
		// it is in the CREATEMODE_MASK, but is not STEREO. It also
		// creates an additional intermediate resource that will be
		// forced to STEREO.

		if (options & ResourceCopyOptions::STEREO) {
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
		} else {
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO);
		}
	}

	switch (src_dimension) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			res = RecreateCompatibleBuffer((ID3D11Buffer*)src_resource, (ID3D11Buffer*)*dst_resource, src_view,
					bind_flags, device, stride, offset, format, buf_src_size, buf_dst_size);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			res = RecreateCompatibleTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, &ID3D11Device::CreateTexture1D>
				((ID3D11Texture1D*)src_resource, (ID3D11Texture1D*)*dst_resource, bind_flags, format,
				 device, mStereoHandle, options);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			res = RecreateCompatibleTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, &ID3D11Device::CreateTexture2D>
				((ID3D11Texture2D*)src_resource, (ID3D11Texture2D*)*dst_resource, bind_flags, format,
				 device, mStereoHandle, options);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			res = RecreateCompatibleTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, &ID3D11Device::CreateTexture3D>
				((ID3D11Texture3D*)src_resource, (ID3D11Texture3D*)*dst_resource, bind_flags, format,
				 device, mStereoHandle, options);
			break;
	}

	if (options & ResourceCopyOptions::CREATEMODE_MASK)
		NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, orig_mode);

	if (res) {
		if (*dst_resource)
			(*dst_resource)->Release();
		if (dst_view && *dst_view)
			(*dst_view)->Release();

		*dst_resource = res;
		if (dst_view)
			*dst_view = NULL;
	}
}

template <typename DescType>
static void FillOutBufferDescCommon(DescType *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	// The documentation on the buffer part of the description is
	// misleading.
	//
	// There are two unions with two possible parameters each which
	// are documented in MSDN, but DX11 never uses ElementWidth
	// (which is determined by either the format, or buffer's
	// StructureByteStride), only NumElements.
	//
	// My reading of FirstElement/ElementOffset sound like they are
	// the same thing, but one is in bytes and the other is in
	// elements - only the names seem backwards compared to the
	// description in the documentation. Research suggests DX11
	// only uses multiples of the element size (since it's a union,
	// it shouldn't matter which name we use).
	//
	// XXX: At the moment we are relying on the region copy to have
	// knocked out the offset for us. We could alternatively do it
	// here (and the below should work), but we would need to
	// create a new view every time the offset changes.
	desc->Buffer.FirstElement = offset / stride;
	desc->Buffer.NumElements = (buf_src_size - offset) / stride;
}

static D3D11_SHADER_RESOURCE_VIEW_DESC* FillOutBufferDesc(
		D3D11_SHADER_RESOURCE_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	// TODO: Also handle BUFFEREX for raw buffers
	desc->ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

	FillOutBufferDescCommon<D3D11_SHADER_RESOURCE_VIEW_DESC>(desc, stride, offset, buf_src_size);
	return desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* FillOutBufferDesc(
		D3D11_RENDER_TARGET_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	desc->ViewDimension = D3D11_RTV_DIMENSION_BUFFER;

	FillOutBufferDescCommon<D3D11_RENDER_TARGET_VIEW_DESC>(desc, stride, offset, buf_src_size);
	return desc;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* FillOutBufferDesc(
		D3D11_UNORDERED_ACCESS_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	desc->ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	// TODO Support buffer UAV flags for append, counter and raw buffers.
	desc->Buffer.Flags = 0;

	FillOutBufferDescCommon<D3D11_UNORDERED_ACCESS_VIEW_DESC>(desc, stride, offset, buf_src_size);
	return desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* FillOutBufferDesc(
		D3D11_DEPTH_STENCIL_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	// Depth views don't support buffers:
	return NULL;
}

template <typename ViewType,
	 typename DescType,
	 HRESULT (__stdcall ID3D11Device::*CreateView)(THIS_
			 ID3D11Resource *pResource,
			 const DescType *pDesc,
			 ViewType **ppView)
	>
static ID3D11View* _CreateCompatibleView(
		ID3D11Resource *resource,
		ID3D11Device *device,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_src_size)
{
	D3D11_RESOURCE_DIMENSION dimension;
	ViewType *view = NULL;
	DescType desc, *pDesc = NULL;
	HRESULT hr;

	resource->GetType(&dimension);
	if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
		// In the case of a buffer type resource we must specify the
		// description as DirectX doesn't have enough information from the
		// buffer alone to create a view.

		desc.Format = format;

		pDesc = FillOutBufferDesc(&desc, stride, offset, buf_src_size);

		// There are a lot of different things we could do with buffer
		// resources, and for now this only covers the structured buffer case.
		// TODO: Handle copying from one type of view to a different type
		// TODO: Handle copying index buffer (with a format) to a view
	}

	hr = (device->*CreateView)(resource, pDesc, &view);
	if (FAILED(hr)) {
		LogInfo("Resource copy CreateCompatibleView failed: %x\n", hr);
		return NULL;
	}

	return view;
}

static ID3D11View* CreateCompatibleView(
		ResourceCopyTarget *dst,
		ID3D11Resource *resource,
		ID3D11Device *device,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_src_size)
{
	switch (dst->type) {
		case ResourceCopyTargetType::SHADER_RESOURCE:
			return _CreateCompatibleView<ID3D11ShaderResourceView,
			       D3D11_SHADER_RESOURCE_VIEW_DESC,
			       &ID3D11Device::CreateShaderResourceView>
				       (resource, device, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::RENDER_TARGET:
			return _CreateCompatibleView<ID3D11RenderTargetView,
			       D3D11_RENDER_TARGET_VIEW_DESC,
			       &ID3D11Device::CreateRenderTargetView>
				       (resource, device, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			return _CreateCompatibleView<ID3D11DepthStencilView,
			       D3D11_DEPTH_STENCIL_VIEW_DESC,
			       &ID3D11Device::CreateDepthStencilView>
				       (resource, device, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			return _CreateCompatibleView<ID3D11UnorderedAccessView,
			       D3D11_UNORDERED_ACCESS_VIEW_DESC,
			       &ID3D11Device::CreateUnorderedAccessView>
				       (resource, device, stride, offset, format, buf_src_size);
	}
	return NULL;
}

ResourceCopyOperation::ResourceCopyOperation() :
	options(ResourceCopyOptions::INVALID),
	cached_resource(NULL),
	cached_view(NULL),
	stereo2mono_intermediate(NULL)
{}

ResourceCopyOperation::~ResourceCopyOperation()
{
	if (cached_resource)
		cached_resource->Release();

	if (cached_view)
		cached_view->Release();
}

static void ResolveMSAA(ID3D11Resource *dst_resource, ID3D11Resource *src_resource,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext)
{
	UINT item, level, index, support;
	D3D11_RESOURCE_DIMENSION dst_dimension;
	ID3D11Texture2D *src, *dst;
	D3D11_TEXTURE2D_DESC desc;
	DXGI_FORMAT fmt;
	HRESULT hr;

	dst_resource->GetType(&dst_dimension);
	if (dst_dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return;

	src = (ID3D11Texture2D*)src_resource;
	dst = (ID3D11Texture2D*)dst_resource;

	dst->GetDesc(&desc);
	fmt = EnsureNotTypeless(desc.Format);

	hr = mOrigDevice->CheckFormatSupport( fmt, &support );
	if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
		// TODO: Implement a fallback using a SM5 shader to resolve it
		LogInfo("Resource copy cannot resolve MSAA format %d\n", fmt);
		return;
	}

	for (item = 0; item < desc.ArraySize; item++) {
		for (level = 0; level < desc.MipLevels; level++) {
			index = D3D11CalcSubresource(level, item, max(desc.MipLevels, 1));
			mOrigContext->ResolveSubresource(dst, index, src, index, fmt);
		}
	}
}

static void ReverseStereoBlit(ID3D11Resource *dst_resource, ID3D11Resource *src_resource,
		StereoHandle mStereoHandle, ID3D11DeviceContext *mOrigContext)
{
	NvAPI_Status nvret;
	D3D11_RESOURCE_DIMENSION src_dimension;
	ID3D11Texture2D *src;
	D3D11_TEXTURE2D_DESC srcDesc;
	UINT item, level, index, width, height;
	D3D11_BOX srcBox;
	int fallbackside, fallback = 0;

	src_resource->GetType(&src_dimension);
	if (src_dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
		// TODO: I think it should be possible to do this with all
		// resource types (possibly including buffers from the
		// discovery of the stereo parameters in the cb12 slot), but I
		// need to test it and make sure it works first
		LogInfo("Resource copy: Reverse stereo blit not supported on resource type %d\n", src_dimension);
		return;
	}

	src = (ID3D11Texture2D*)src_resource;
	src->GetDesc(&srcDesc);

	// TODO: Resolve MSAA
	// TODO: Use intermediate resource if copying from a texture with depth buffer bind flags

	nvret = NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, true);
	if (nvret != NVAPI_OK) {
		LogInfo("Resource copying failed to enable reverse stereo blit\n");
		// Fallback path: Copy 2D resource to both sides of the 2x
		// width destination (TESTME)
		fallback = 1;
	}

	for (fallbackside = 0; fallbackside < 1 + fallback; fallbackside++) {

		// Set the source box as per the nvapi documentation:
		srcBox.left = 0;
		srcBox.top = 0;
		srcBox.front = 0;
		srcBox.right = width = srcDesc.Width;
		srcBox.bottom = height = srcDesc.Height;
		srcBox.back = 1;

		// Perform the reverse stereo blit on all sub-resources and mip-maps:
		for (item = 0; item < srcDesc.ArraySize; item++) {
			for (level = 0; level < srcDesc.MipLevels; level++) {
				index = D3D11CalcSubresource(level, item, max(srcDesc.MipLevels, 1));
				srcBox.right = width >> level;
				srcBox.bottom = height >> level;
				mOrigContext->CopySubresourceRegion(dst_resource, index,
						fallbackside * srcBox.right, 0, 0,
						src, index, &srcBox);
			}
		}
	}

	NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, false);
}

static void SpecialCopyBufferRegion(ID3D11Resource *dst_resource,ID3D11Resource *src_resource,
		ID3D11DeviceContext *mOrigContext, UINT stride, UINT *offset,
		UINT buf_src_size, UINT buf_dst_size)
{
	// We are copying a buffer for use in a constant buffer and the size of
	// the original buffer did not meet the constraints of a constant
	// buffer.
	D3D11_BOX src_box;

	// We want to copy from the offset to the end of the source buffer, but
	// cap it to the destination size to avoid "undefined behaviour". Keep
	// in mind that this is "right", not "size":
	src_box.left = *offset;
	src_box.right = min(buf_src_size, *offset + buf_dst_size);

	if (stride) {
		// If we are copying to a structured resource, the source box
		// must be a multiple of the stride, so round it down:
		src_box.right = (src_box.right - src_box.left) / stride * stride + src_box.left;
	}

	src_box.top = 0;
	src_box.bottom = 1;
	src_box.front = 0;
	src_box.back = 1;

	mOrigContext->CopySubresourceRegion(dst_resource, 0, 0, 0, 0, src_resource, 0, &src_box);

	// We have effectively removed the offset during the region copy, so
	// set it to 0 to make sure nothing will try to use it again elsewhere:
	*offset = 0;
}

// Is there already a utility function that does this?
static UINT dxgi_format_size(DXGI_FORMAT format)
{
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return 16;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return 12;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return 8;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return 4;
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return 2;
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
			return 1;
		default:
			return 0;
	}
}

void ResourceCopyOperation::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	ID3D11Resource *src_resource = NULL;
	ID3D11Resource *dst_resource = NULL;
	ID3D11Resource **pp_cached_resource = &cached_resource;
	ID3D11View *src_view = NULL;
	ID3D11View *dst_view = NULL;
	ID3D11View **pp_cached_view = &cached_view;
	UINT stride = 0;
	UINT offset = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	UINT buf_src_size = 0, buf_dst_size = 0;

	if (src.type == ResourceCopyTargetType::EMPTY) {
		dst.SetResource(mOrigContext, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN);
		return;
	}

	src_resource = src.GetResource(mOrigDevice, mOrigContext, &src_view, &stride, &offset, &format);
	if (!src_resource) {
		LogDebug("Resource copy: Source was NULL\n");
		if (!(options & ResourceCopyOptions::UNLESS_NULL)) {
			// Still set destination to NULL - if we are copying a
			// resource we generally expect it to be there, and
			// this will make errors more obvious if we copy
			// something that doesn't exist. This behaviour can be
			// overridden with the unless_null keyword.
			dst.SetResource(mOrigContext, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN);
		}
		return;
	}

	if (dst.type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
		// If we're copying to a custom resource, use the resource &
		// view in the CustomResource directly as the cache instead of
		// the cache in the ResourceCopyOperation. This will reduce the
		// number of extra resources we have floating around if copying
		// something to a single custom resource from multiple shaders.
		pp_cached_resource = &dst.custom_resource->resource;
		pp_cached_view = &dst.custom_resource->view;

		if (dst.custom_resource->max_copies_per_frame) {
			if (dst.custom_resource->frame_no != G->frame_no) {
				dst.custom_resource->frame_no = G->frame_no;
				dst.custom_resource->copies_this_frame = 0;
			} else if (dst.custom_resource->copies_this_frame++ >= dst.custom_resource->max_copies_per_frame)
				return;
		}
	}

	if (format == DXGI_FORMAT_UNKNOWN)
		format = GetViewFormat(src.type, src_view);
	if (!stride)
		stride = dxgi_format_size(format);

	if (options & ResourceCopyOptions::COPY_MASK) {
		RecreateCompatibleResource(&dst, src_resource,
			pp_cached_resource, src_view, pp_cached_view,
			mOrigDevice, mHackerDevice->mStereoHandle,
			options, stride, offset, format,
			&buf_src_size, &buf_dst_size);

		if (!*pp_cached_resource) {
			LogInfo("Resource copy error: Could not create/update destination resource\n");
			goto out_release;
		}
		dst_resource = *pp_cached_resource;
		dst_view = *pp_cached_view;

		if (options & ResourceCopyOptions::STEREO2MONO) {

			// TODO: Resolve MSAA to an intermediate resource first
			// if necessary (but keep in mind this may have
			// compatibility issues without a fallback path)

			// The reverse stereo blit seems to only work if the
			// destination resource is stereo. This is a bit
			// bizzare since the whole point of it is to create a
			// double width mono resource, but there you go.
			// We use a second intermediate resource that is forced
			// to stereo and the final destination is forced to
			// mono - once we have done the reverse blit we use an
			// ordinary copy to the final mono resource.

			RecreateCompatibleResource(NULL, src_resource,
				&stereo2mono_intermediate, NULL, NULL,
				mOrigDevice, mHackerDevice->mStereoHandle,
				(ResourceCopyOptions)(options | ResourceCopyOptions::STEREO),
				stride, offset, format, NULL, NULL);

			ReverseStereoBlit(stereo2mono_intermediate, src_resource,
					mHackerDevice->mStereoHandle, mOrigContext);

			mOrigContext->CopyResource(dst_resource, stereo2mono_intermediate);

		} else if (options & ResourceCopyOptions::RESOLVE_MSAA) {
			ResolveMSAA(dst_resource, src_resource, mOrigDevice, mOrigContext);
		} else if (buf_dst_size) {
			SpecialCopyBufferRegion(dst_resource, src_resource,
					mOrigContext, stride, &offset,
					buf_src_size, buf_dst_size);
		} else {
			mOrigContext->CopyResource(dst_resource, src_resource);
		}
	} else {
		dst_resource = src_resource;
		if (src_view && (src.type == dst.type))
			dst_view = src_view;
		else
			dst_view = *pp_cached_view;
		// TODO: If we are referencing to/from a custom resource we
		// currently don't reference the view, but we could so long as
		// the bind flags from the original source are compatible with
		// the bind flags in the final destination. If we implement
		// this, go read the note in CustomResource::Substantiate()
	}

	if (!dst_view) {
		dst_view = CreateCompatibleView(&dst, dst_resource, mOrigDevice,
				stride, offset, format, buf_src_size);
		// Not checking for NULL return as view's are not applicable to
		// all types. TODO: Check for legitimate failures.
		*pp_cached_view = dst_view;
	}

	dst.SetResource(mOrigContext, dst_resource, dst_view, stride, offset, format);

out_release:
	src_resource->Release();
	if (src_view)
		src_view->Release();
}
