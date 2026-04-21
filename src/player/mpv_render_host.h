#pragma once

class QOpenGLContext;

namespace shatv::player {

class MpvRenderHost {
   public:
    virtual ~MpvRenderHost() = default;

    virtual QOpenGLContext *CurrentContext() const = 0;
    virtual void MakeCurrent() = 0;
    virtual void DoneCurrent() = 0;
    virtual void RequestUpdate() = 0;
};

}  // namespace shatv::player
