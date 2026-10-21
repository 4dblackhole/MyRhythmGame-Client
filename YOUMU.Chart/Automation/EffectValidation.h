#pragma once
#include "Model/Submodules/EffectDocument.h"

class MusicalTimeline;
// Shared semantic rules for file parsing, editing and session construction.
void ValidateEffectValue(EffectCommandType type, double value);
void ValidateEffectCommand(const EffectCommand &command);
void ResolveEffectInterpolations(EffectDocument &document);
void ValidateEffectDocument(const EffectDocument &document);
void ValidateEffectPositions(const EffectDocument &document, const MusicalTimeline &timeline);
