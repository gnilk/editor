//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

class ScreenBase {
        public:
        ScreenBase() = default;
        virtual ~ScreenBase() = default;
        virtual bool Open() { return false; }
        virtual void Close() { }
        virtual void Clear() { }
        virtual void Update() { }
        void InvalidateAll() { invalidateAll = true; }
        protected:
        bool invalidateAll = false;
};

#endif //EDITOR_SCREENBASE_H
