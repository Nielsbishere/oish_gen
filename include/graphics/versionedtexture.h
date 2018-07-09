#pragma once
#include "texture.h"

namespace oi {

	namespace gc {

		struct VersionedTextureInfo {

			u32 versions = 0;
			std::vector<Texture*> version;

			VersionedTextureInfo(std::vector<Texture*> version) : versions((u32)version.size()), version(version) {}
			VersionedTextureInfo() {}

		};

		class VersionedTexture : public GraphicsResource {

			friend class Graphics;

		public:

			TextureFormat getFormat();
			TextureUsage getUsage();
			Vec2u getSize();
			u32 getVersions();
			bool isOwned();

			Texture *getVersion(u32 i);

			const VersionedTextureInfo getInfo();

		protected:

			~VersionedTexture();
			VersionedTexture(VersionedTextureInfo info);
			bool init();

		private:

			VersionedTextureInfo info;

		};

	}

}