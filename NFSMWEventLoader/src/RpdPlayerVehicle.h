#pragma once
// Private integration within this module; not a third-party loader ABI.
void* MWELRpdPlayerCollection(unsigned classKey,unsigned key);
bool MWELRpdPlayerSpecification(unsigned sourceKey,void* customization,unsigned* playerKey);
void MWELRpdPrepareVehicles();
bool MWELRpdPresetAvailable(const std::string& preset);
std::string MWELRpdLoanPreset(const std::string& preset);
unsigned MWELRpdPresetHeat(const std::string& preset);
unsigned MWELRpdVehicleHeat(unsigned vehicleKey);
std::string MWELRpdPresetLabel(const std::string& preset);
