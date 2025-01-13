#ifndef _RIVE_DATA_CONVERTER_TO_STRING_HPP_
#define _RIVE_DATA_CONVERTER_TO_STRING_HPP_
#include "rive/generated/data_bind/converters/data_converter_to_string_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterToString : public DataConverterToStringBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::string; };

private:
    DataValueString m_output;
};
} // namespace rive

#endif