#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>

#include "rpc_options.pb.h"

namespace
{

using google::protobuf::Descriptor;
using google::protobuf::Edition;
using google::protobuf::FileDescriptor;
using google::protobuf::MethodDescriptor;
using google::protobuf::ServiceDescriptor;
using google::protobuf::compiler::CodeGenerator;
using google::protobuf::compiler::GeneratorContext;

class RpcGenerator final : public CodeGenerator
{
public:
    uint64_t GetSupportedFeatures() const override
    {
        return FEATURE_PROTO3_OPTIONAL | FEATURE_SUPPORTS_EDITIONS;
    }
    Edition GetMinimumEdition() const override { return Edition::EDITION_2023; }
    Edition GetMaximumEdition() const override { return Edition::EDITION_2023; }

    bool Generate (const FileDescriptor* file,
                   const std::string& parameter,
                   GeneratorContext* context,
                   std::string* error) const override
    {
        (void) parameter;

        if (file->service_count() == 0)
        {
            return true;
        }

        for (int i = 0; i < file->service_count(); ++i)
        {
            const auto* service = file->service (i);

            if (! ValidateService (*service, error))
            {
                return false;
            }
        }

        GenerateHeader (*file, context);
        GenerateSource (*file, context);

        return true;
    }

private:
    static bool ValidateService (const ServiceDescriptor& service, std::string* error)
    {
        std::unordered_set<std::uint32_t> ids;

        for (int i = 0; i < service.method_count(); ++i)
        {
            const auto* method = service.method (i);

            const auto& options = method->options();

            if (! options.HasExtension (auralreality::rpc_id))
            {
                *error = "RPC method '" + std::string (method->full_name())
                         + "' has no (auralreality.rpc_id)";

                return false;
            }

            const std::uint32_t rpc_id = options.GetExtension (auralreality::rpc_id);

            if (rpc_id == 0)
            {
                *error = "RPC method '" + std::string (method->full_name()) + "' has rpc_id=0";

                return false;
            }

            if (! ids.insert (rpc_id).second)
            {
                *error = "Duplicate rpc_id=" + std::to_string (rpc_id) + " in service '"
                         + std::string (service.full_name()) + "'";

                return false;
            }

            if (method->client_streaming() || method->server_streaming())
            {
                *error =
                    "Streaming RPC '" + std::string (method->full_name()) + "' is not supported";

                return false;
            }

            if (method->input_type() == nullptr || method->output_type() == nullptr)
            {
                *error =
                    "RPC '" + std::string (method->full_name()) + "' has invalid input/output type";

                return false;
            }
        }

        return true;
    }

    static std::uint32_t RpcId (const MethodDescriptor& method)
    {
        return method.options().GetExtension (auralreality::rpc_id);
    }

    static std::string CppNamespace (const FileDescriptor& file)
    {
        std::string package = std::string (file.package());

        if (package.empty())
        {
            return {};
        }

        std::ostringstream out;

        size_t begin = 0;

        while (begin < package.size())
        {
            const size_t end = package.find ('.', begin);

            if (end == std::string::npos)
            {
                out << package.substr (begin);
                break;
            }

            out << package.substr (begin, end - begin);
            out << "::";

            begin = end + 1;
        }

        return out.str();
    }

    static bool IsEmpty (const Descriptor& descriptor) { return descriptor.name() == "Empty"; }

    static std::string CppType (const Descriptor& descriptor)
    {
        std::ostringstream out;

        out << "::";

        const FileDescriptor* file = descriptor.file();

        const std::string package = std::string (file->package());

        if (! package.empty())
        {
            size_t begin = 0;

            while (begin < package.size())
            {
                const size_t end = package.find ('.', begin);

                if (end == std::string::npos)
                {
                    out << package.substr (begin);
                    break;
                }

                out << package.substr (begin, end - begin);

                out << "::";

                begin = end + 1;
            }

            out << "::";
        }

        std::vector<const Descriptor*> nesting;

        for (const Descriptor* d = &descriptor; d != nullptr; d = d->containing_type())
        {
            nesting.push_back (d);
        }

        for (auto it = nesting.rbegin(); it != nesting.rend(); ++it)
        {
            if (it != nesting.rbegin())
            {
                out << "::";
            }

            out << (*it)->name();
        }

        return out.str();
    }

    static std::string HeaderGuard (const FileDescriptor& file)
    {
        std::string result = std::string (file.name());

        for (char& c : result)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = static_cast<char> (c - 'a' + 'A');
            }
            else if (! ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            {
                c = '_';
            }
        }

        return "AURAL_RPC_" + result + "_RPC_H";
    }

    static std::string BaseName (const FileDescriptor& file)
    {
        std::string name = std::string (file.name());

        const auto slash = name.find_last_of ("/");

        if (slash != std::string::npos)
        {
            name.erase (0, slash + 1);
        }

        const auto dot = name.rfind (".proto");

        if (dot != std::string::npos)
        {
            name.erase (dot);
        }

        return name;
    }

    static std::string Indent (const std::string& text, int spaces)
    {
        std::ostringstream out;

        std::string prefix (spaces, ' ');

        size_t start = 0;

        while (start < text.size())
        {
            const size_t end = text.find ('\n', start);

            out << prefix;

            if (end == std::string::npos)
            {
                out << text.substr (start);
                break;
            }

            out << text.substr (start, end - start);
            out << '\n';

            start = end + 1;
        }

        return out.str();
    }

    static void GenerateHeader (const FileDescriptor& file, GeneratorContext* context)
    {
        const std::string base = BaseName (file);

        const std::string filename = base + ".rpc.h";

        auto output = context->Open (filename);

        std::ostringstream out;

        out << "#pragma once\n\n";

        out << "#include <cstddef>\n\n";

        out << "#include <arrpc/arrpc_channel.h>\n";
        out << "#include <arrpc/arrpc_future.h>\n";
        out << "#include <arrpc/arrpc_responder.h>\n\n";

        // include generated protobuf header
        const auto proto_header = base + ".pb.h";
        out << "#include \"" << proto_header << "\" // IWYU pragma: export\n\n";

        const std::string ns = CppNamespace (file);

        if (! ns.empty())
        {
            out << "namespace " << ns << " {\n\n";
        }

        for (int i = 0; i < file.service_count(); ++i)
        {
            const auto* service = file.service (i);

            GenerateServiceHeader (*service, out);
        }

        if (! ns.empty())
        {
            out << "} // namespace " << ns << "\n";
        }

        if (! output->WriteCord (absl::Cord (out.str())))
            throw std::runtime_error ("Failed to write output file: " + filename);
    }

    static void GenerateServiceHeader (const ServiceDescriptor& service, std::ostringstream& out)
    {
        const std::string service_name = std::string (service.name());
        const std::string service_class_name = service_name + "Base";
        const std::string client_class_name = service_name + "Client";

        //
        // Server interface
        //

        out << "class " << service_class_name << " {\n";
        out << "public:\n";

        out << "    virtual ~" << service_class_name << "() = default;\n\n";

        for (int i = 0; i < service.method_count(); ++i)
        {
            const auto* method = service.method (i);

            const std::string input = CppType (*method->input_type());

            const std::string output = CppType (*method->output_type());

            out << "    virtual void " << method->name() << "(\n";

            out << "        const " << input << "& request,\n";

            out << "        ::rpc::RpcResponder<" << output << "> response) = 0;\n\n";
        }

        out << "};\n\n";

        //
        // Client
        //

        out << "class " << client_class_name << " {\n";

        out << "public:\n";

        out << "    explicit " << client_class_name << "("
            << "::rpc::RpcChannel& channel);\n\n";

        for (int i = 0; i < service.method_count(); ++i)
        {
            const auto* method = service.method (i);

            const std::string input = CppType (*method->input_type());

            const std::string output = CppType (*method->output_type());

            out << "    ::rpc::RpcFuture<" << output << "> " << method->name() << "(\n";

            if (! IsEmpty (*method->input_type()))
            {
                out << "        const " << input << "& request";
            }

            out << ");\n\n";
        }

        out << "private:\n";

        out << "    ::rpc::RpcChannel& channel_;\n";

        out << "};\n\n";

        //
        // Dispatcher
        //

        out << "class " << service_name << "Dispatcher {\n";

        out << "public:\n";

        out << "    explicit " << service_name << "Dispatcher(" << service_class_name
            << "& service)\n";

        out << "        : service_(service) {}\n\n";

        out << "    void dispatch(\n";
        out << "        ::rpc::RpcServerConnection& connection,\n";
        out << "        ::rpc::RpcId rpc_id,\n";
        out << "        ::rpc::RequestId request_id,\n";
        out << "        const void* data,\n";
        out << "        std::size_t size);\n\n";

        out << "private:\n";

        out << "    " << service_class_name << "& service_;\n";

        out << "};\n\n";
    }

    static void GenerateSource (const FileDescriptor& file, GeneratorContext* context)
    {
        const std::string base = BaseName (file);

        auto output = context->Open (base + ".rpc.cc");

        std::ostringstream out;

        out << "#include \"" << base << ".rpc.h\"\n\n";

        out << "#include <cstring>\n";
        out << "#include <string>\n\n";

        out << "#include <google/protobuf/message_lite.h>\n\n";

        const std::string ns = CppNamespace (file);

        if (! ns.empty())
        {
            out << "namespace " << ns << " {\n\n";
        }

        for (int i = 0; i < file.service_count(); ++i)
        {
            const auto* service = file.service (i);

            GenerateServiceSource (*service, out);
        }

        if (! ns.empty())
        {
            out << "} // namespace " << ns << "\n";
        }

        if (! output->WriteCord (absl::Cord (out.str())))
            throw std::runtime_error ("Failed to write output file: " + base + ".rpc.cc");
    }

    static void GenerateServiceSource (const ServiceDescriptor& service, std::ostringstream& out)
    {
        const std::string name = std::string (service.name());

        //
        // Client constructor
        //

        out << name << "Client::" << name << "Client("
            << "::rpc::RpcChannel& channel)\n";

        out << "    : channel_(channel) {}\n\n";

        //
        // Client methods
        //

        for (int i = 0; i < service.method_count(); ++i)
        {
            const auto* method = service.method (i);

            const std::string output = CppType (*method->output_type());

            const std::string input = CppType (*method->input_type());
            bool isEmpty = IsEmpty (*method->input_type());

            const std::uint32_t rpc_id = RpcId (*method);

            out << "::rpc::RpcFuture<" << output << ">\n";

            out << name << "Client::" << method->name() << "(\n";

            if (! isEmpty)
            {
                out << "    const " << input << "& request";
            }
            out << ")\n";

            out << "{\n";

            if (isEmpty)
            {
                out << "    " << input << " request;";
            }

            out << "    return channel_.call<" << output << ">(\n";

            out << "        " << rpc_id << ",\n";

            out << "        request);\n";

            out << "}\n\n";
        }

        //
        // Dispatcher
        //

        out << "void " << name << "Dispatcher::dispatch(\n";

        out << "    ::rpc::RpcServerConnection& connection,\n";
        out << "    ::rpc::RpcId rpc_id,\n";
        out << "    ::rpc::RequestId request_id,\n";
        out << "    const void* data,\n";
        out << "    std::size_t size)\n";

        out << "{\n";

        out << "    switch (rpc_id) {\n\n";

        for (int i = 0; i < service.method_count(); ++i)
        {
            const auto* method = service.method (i);

            GenerateDispatchCase (*method, out);
        }

        out << "    default:\n";

        out << "        connection.respond_error(\n";
        out << "            request_id,\n";
        out << "            ::rpc::RpcStatus::error(\n";
        out << "                ::rpc::RpcStatus::Code::"
            << "UnknownMethod,\n";

        out << "                \"Unknown RPC method\"));\n";

        out << "        break;\n";

        out << "    }\n";
        out << "}\n\n";
    }

    static void GenerateDispatchCase (const MethodDescriptor& method, std::ostringstream& out)
    {
        const std::uint32_t rpc_id = RpcId (method);

        const std::string input = CppType (*method.input_type());

        const std::string output = CppType (*method.output_type());

        out << "    case " << rpc_id << ": {\n";

        out << "        " << input << " request;\n\n";

        out << "        if (!request.ParseFromArray(\n";
        out << "                data,\n";
        out << "                static_cast<int>(size))) {\n\n";

        out << "            connection.respond_error(\n";
        out << "                request_id,\n";

        out << "                ::rpc::RpcStatus::error(\n";

        out << "                    ::rpc::RpcStatus::Code::"
            << "InvalidArgument,\n";

        out << "                    \"Invalid protobuf request\"));\n";

        out << "            break;\n";
        out << "        }\n\n";

        out << "        service_." << method.name() << "(\n";

        out << "            request,\n";

        out << "            ::rpc::RpcResponder<" << output << ">(\n";

        out << "                connection,\n";
        out << "                request_id));\n";

        out << "        break;\n";
        out << "    }\n\n";
    }
};

} // namespace

int main (int argc, char** argv)
{
    RpcGenerator generator;

    return google::protobuf::compiler::PluginMain (argc, argv, &generator);
}
