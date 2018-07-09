#pragma once
#include <types/generic.h>
#include <types/string.h>
#include <typeinfo>

namespace oi {

	namespace gc {

		class Graphics;

		class GraphicsObject { 
		
			friend class Graphics;

		public:

			virtual ~GraphicsObject();

			size_t getHash() const;

			String getName() const;
			String getTypeName() const;

		protected:

			Graphics *g = nullptr;
			i32 refCount = 0;

			template<typename T>
			void setHash() {

				hash = typeid(T).hash_code();
				
				auto it = names.find(hash);

				if (it == names.end())
					names[hash] = typeid(T).name();

			}

		private:

			size_t hash = (size_t) -1;
			String name;

			static std::unordered_map<size_t, String> names;


		};

	}
}