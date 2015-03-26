// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/math.h"

RasterizerCacheOpenGL::RasterizerCacheOpenGL(ResourceManagerOpenGL* res_mgr) : res_mgr(res_mgr) {
}

RasterizerCacheOpenGL::~RasterizerCacheOpenGL() {
    FullFlush();
}

void RasterizerCacheOpenGL::LoadAndBindTexture(Pica::Regs::FullTextureConfig config) {
    u32 tex_paddr = config.config.GetPhysicalAddress();

    auto cached_texture = texture_cache.find(tex_paddr);

    if (cached_texture != texture_cache.end()) {
        glBindTexture(GL_TEXTURE_2D, cached_texture->second.handle);
    } else {
        CachedTexture new_texture;

        new_texture.handle = res_mgr->NewTexture();
        glBindTexture(GL_TEXTURE_2D, new_texture.handle);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        switch (config.config.wrap_s.Value()) {
        case Pica::Regs::TextureConfig::WrapMode::ClampToEdge:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            break;

        case Pica::Regs::TextureConfig::WrapMode::Repeat:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            break;

        case Pica::Regs::TextureConfig::WrapMode::MirroredRepeat:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            break;

        default:
            LOG_ERROR(Render_OpenGL, "Unknown texture wrap_s mode %d", config.config.wrap_s.Value());
            break;
        }

        switch (config.config.wrap_t.Value()) {
        case Pica::Regs::TextureConfig::WrapMode::ClampToEdge:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;

        case Pica::Regs::TextureConfig::WrapMode::Repeat:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            break;

        case Pica::Regs::TextureConfig::WrapMode::MirroredRepeat:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            break;

        default:
            LOG_ERROR(Render_OpenGL, "Unknown texture wrap_t mode %d", config.config.wrap_t.Value());
            break;
        }

        auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config.config, config.format);

        new_texture.width = info.width;
        new_texture.height = info.height;
        new_texture.size = info.width * info.height * Pica::Regs::NibblesPerPixel(info.format);

        Math::Vec4<u8>* rgba_tex = new Math::Vec4<u8>[info.width * info.height];

        for (int i = 0; i < info.width; i++)
        {
            for (int j = 0; j < info.height; j++)
            {
                rgba_tex[i + info.width * j] = Pica::DebugUtils::LookupTexture(Memory::GetPointer(Pica::PAddrToVAddr(tex_paddr)), i, info.height - 1 - j, info);
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.width, info.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_tex);

        delete rgba_tex;

        texture_cache.emplace(tex_paddr, new_texture);
    }
}

void RasterizerCacheOpenGL::NotifyFlush(bool is_phys_addr, u32 addr, u32 size) {
    // Flush any texture that falls in the flushed region
    for (auto it = texture_cache.begin(); it != texture_cache.end();) {
        u32 tex_comparison_addr = is_phys_addr ? it->first : Pica::PAddrToVAddr(it->first);

        u32 max_lower = std::max(addr, tex_comparison_addr);
        u32 min_upper = std::min(addr + size, tex_comparison_addr + it->second.size);

        if (max_lower <= min_upper) {
            res_mgr->DeleteTexture(it->second.handle);
            it = texture_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void RasterizerCacheOpenGL::FullFlush() {
    for (auto texture : texture_cache) {
        res_mgr->DeleteTexture(texture.second.handle);
    }
}
