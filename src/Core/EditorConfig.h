//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_EDITORCONFIG_H
#define EDITOR_EDITORCONFIG_H

class EditorConfig {
        public:
        static EditorConfig &Instance() {
            static EditorConfig config;
            return config;
        }
        private:
        EditorConfig() = default;
        public:
        int32_t tabSize = 4;
};


#endif //EDITOR_EDITORCONFIG_H
