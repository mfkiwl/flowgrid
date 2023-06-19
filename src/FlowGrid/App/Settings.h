#pragma once

#include "Core/Primitive/Float.h"

struct ApplicationSettings : Component, Drawable {
    using Component::Component;

    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture

protected:
    void Render() const override;
};

extern const ApplicationSettings &application_settings;
