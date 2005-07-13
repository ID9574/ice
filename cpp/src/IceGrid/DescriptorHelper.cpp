// **********************************************************************
//
// Copyright (c) 2003-2005 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/Ice.h>
#include <IceGrid/DescriptorHelper.h>
#include <IceGrid/Util.h>

#include <iterator>

using namespace std;
using namespace IceGrid;

namespace IceGrid
{

struct Substitute : unary_function<string&, void>
{
    Substitute(const DescriptorVariablesPtr& variables, set<string>& missing) : 
	_variables(variables), _missing(missing)
    {
    } 

    void operator()(string& v)
    {
	v.assign(_variables->substituteWithMissing(v, _missing));
    }

    const DescriptorVariablesPtr& _variables;
    set<string>& _missing;
};

struct GetReplicatedAdapterId : unary_function<ReplicatedAdapterDescriptor&, const string&>
{
    const string&
    operator()(const ReplicatedAdapterDescriptor& desc)
    {
	return desc.id;
    }
};

struct GetNodeName : unary_function<NodeDescriptor&, const string&>
{
    const string&
    operator()(const NodeDescriptor& desc)
    {
	return desc.name;
    }
};

struct GetServerName : unary_function<ServerInstanceDescriptor&, const string&>
{
    const string&
    operator()(const ServerInstanceDescriptor& desc)
    {
	assert(desc.descriptor);
	return desc.descriptor->name;
    }
};

struct ServerDescriptorEqual : std::binary_function<ServerDescriptorPtr&, ServerDescriptorPtr&, bool>
{
    ServerDescriptorEqual(ApplicationDescriptorHelper& helper) : _helper(helper)
    {
    }
    
    bool
    operator()(const ServerDescriptorPtr& lhs, const ServerDescriptorPtr& rhs)
    {
	return ServerDescriptorHelper(_helper, lhs) == ServerDescriptorHelper(_helper, rhs);
    }

    ApplicationDescriptorHelper& _helper;
};

struct ServiceDescriptorEqual : std::binary_function<ServiceDescriptorPtr&, ServiceDescriptorPtr&, bool>
{
    ServiceDescriptorEqual(ApplicationDescriptorHelper& helper) : _helper(helper)
    {
    }
    
    bool
    operator()(const ServiceDescriptorPtr& lhs, const ServiceDescriptorPtr& rhs)
    {
	return ServiceDescriptorHelper(_helper, lhs) == ServiceDescriptorHelper(_helper, rhs);
    }

    ApplicationDescriptorHelper& _helper;
};

struct TemplateDescriptorEqual : std::binary_function<TemplateDescriptor&, TemplateDescriptor&, bool>
{
    TemplateDescriptorEqual(ApplicationDescriptorHelper& helper) : _helper(helper)
    {
    }
    
    bool
    operator()(const TemplateDescriptor& lhs, const TemplateDescriptor& rhs)
    {
	if(lhs.parameters != rhs.parameters)
	{
	    return false;
	}
	
	ServerDescriptorPtr slhs = ServerDescriptorPtr::dynamicCast(lhs.descriptor);
	ServerDescriptorPtr srhs = ServerDescriptorPtr::dynamicCast(lhs.descriptor);
	if(slhs && srhs)
	{
	    return ServerDescriptorHelper(_helper, slhs) == ServerDescriptorHelper(_helper, srhs);
	}

	ServiceDescriptorPtr svclhs = ServiceDescriptorPtr::dynamicCast(lhs.descriptor);
	ServiceDescriptorPtr svcrhs = ServiceDescriptorPtr::dynamicCast(lhs.descriptor);
	if(svclhs && svcrhs)
	{
	    return ServiceDescriptorHelper(_helper, svclhs) == ServiceDescriptorHelper(_helper, svcrhs);
	}

	return false;
    }

    ApplicationDescriptorHelper& _helper;
};

struct ServerInstanceDescriptorEqual : std::binary_function<ServerInstanceDescriptor&, 
							    ServerInstanceDescriptor&, 
							    bool>
{
    ServerInstanceDescriptorEqual(ApplicationDescriptorHelper& helper) : _helper(helper)
    {
    }
    
    bool
    operator()(const ServerInstanceDescriptor& lhs, const ServerInstanceDescriptor& rhs)
    {
	if(lhs._cpp_template != rhs._cpp_template)
	{
	    return false;
	}
	if(lhs.parameterValues != rhs.parameterValues)
	{
	    return false;
	}
	if(lhs.targets != rhs.targets)
	{
	    return false;
	}
	return ServerDescriptorHelper(_helper, lhs.descriptor) == ServerDescriptorHelper(_helper, rhs.descriptor);
    }

    ApplicationDescriptorHelper& _helper;
};

template <typename GetKeyFunc, typename Seq> Seq
getSeqUpdatedElts(const Seq& lseq, const Seq& rseq, GetKeyFunc func)
{
    return getSeqUpdatedElts(lseq, rseq, func, equal_to<typename Seq::value_type>());
}

template <typename GetKeyFunc, typename EqFunc, typename Seq> Seq
getSeqUpdatedElts(const Seq& lseq, const Seq& rseq, GetKeyFunc func, EqFunc eq)
{
    Seq result;
    for(typename Seq::const_iterator p = rseq.begin(); p != rseq.end(); ++p)
    {
	typename Seq::const_iterator q = lseq.begin();
	for(; q != lseq.end(); ++q)
	{
	    if(func(*p) == func(*q))
	    {
		break;
	    }
	}
	if(q == lseq.end() || !eq(*p, *q))
	{
	    result.push_back(*p);
	}
    }
    return result;
}

template <typename GetKeyFunc, typename Seq> Ice::StringSeq 
getSeqRemovedElts(const Seq& lseq, const Seq& rseq, GetKeyFunc func)
{
    Ice::StringSeq removed;
    for(typename Seq::const_iterator p = lseq.begin(); p != lseq.end(); ++p)
    {
	typename Seq::const_iterator q;
	for(q = rseq.begin(); q != rseq.end(); ++q)
	{
	    if(func(*p) == func(*q))
	    {
		break;
	    }
	}
	if(q == rseq.end())
	{
	    removed.push_back(func(*p));
	}
    }
    return removed;
}

template <typename GetKeyFunc, typename Seq> Seq
updateSeqElts(const Seq& seq, const Seq& update, const Ice::StringSeq& remove, GetKeyFunc func)
{
    Seq result = update;
    set<string> removed(remove.begin(), remove.end());
    for(typename Seq::const_iterator p = seq.begin(); p != seq.end(); ++p)
    {
	if(removed.find(func(*p)) == removed.end())
	{
	    typename Seq::const_iterator q = update.begin();
	    for(; q != update.end(); ++q)
	    {
		if(func(*p) == func(*q))
		{
		    break;
		}
	    }
	    if(q == update.end())
	    {
		result.push_back(*p);
	    }
	}
    }
    return result;
}

template<typename Dict> Dict
getDictUpdatedElts(const Dict& ldict, const Dict& rdict)
{
    return getDictUpdatedElts(ldict, rdict, equal_to<typename Dict::mapped_type>());
}

template<typename Dict, typename EqFunc> Dict
getDictUpdatedElts(const Dict& ldict, const Dict& rdict, EqFunc eq)
{
    Dict result;
    for(typename Dict::const_iterator p = rdict.begin(); p != rdict.end(); ++p)
    {
	typename Dict::const_iterator q = ldict.find(p->first);
	if(q == ldict.end() || !eq(p->second, q->second))
	{
	    result.insert(*p);
	}
    }
    return result;
}

template <typename Dict> Ice::StringSeq
getDictRemovedElts(const Dict& ldict, const Dict& rdict)
{
    Ice::StringSeq removed;
    for(typename Dict::const_iterator p = ldict.begin(); p != ldict.end(); ++p)
    {
	if(rdict.find(p->first) == rdict.end())
	{
	    removed.push_back(p->first);
	}
    }
    return removed;
}

template <typename Dict> Dict
updateDictElts(const Dict& dict, const Dict& update, const Ice::StringSeq& remove)
{
    Dict result = dict;
    for(Ice::StringSeq::const_iterator p = remove.begin(); p != remove.end(); ++p)
    {
	result.erase(*p);
    }
    for(typename Dict::const_iterator q = update.begin(); q != update.end(); ++q)
    {
	result[q->first] = q->second;
    }
    return result;
}

}

DescriptorVariables::DescriptorVariables()
{
}

DescriptorVariables::DescriptorVariables(const map<string, string>& variables)
{
    reset(variables, vector<string>());
}

string 
DescriptorVariables::substitute(const string& v)
{
    set<string> missing;
    string value = substituteImpl(v, missing);
    if(!missing.empty())
    {
	if(missing.size() == 1)
	{
	    throw "unknown variable `" + *missing.begin() + "'";
	}
	else
	{
	    ostringstream os;
	    os << "unknown variables: ";
	    copy(missing.begin(), missing.end(), ostream_iterator<string>(os, " "));
	    throw os.str();
	}
    }
    return value;
}

string
DescriptorVariables::substituteWithMissing(const string& v, set<string>& missing)
{
    return substituteImpl(v, missing);
}

void
DescriptorVariables::dumpVariables() const
{
    vector<VariableScope>::const_reverse_iterator p = _scopes.rbegin();
    while(p != _scopes.rend())
    {
	for(map<string, string>::const_iterator q = p->variables.begin(); q != p->variables.end(); ++q)
	{
	    cout << q->first << " = " << q->second << endl;
	}
	++p;
    }
}

string
DescriptorVariables::getVariable(const string& name)
{
    static const string empty;
    vector<VariableScope>::reverse_iterator p = _scopes.rbegin();
    while(p != _scopes.rend())
    {
	map<string, string>::const_iterator q = p->variables.find(name);
	if(q != p->variables.end())
	{
	    p->used.insert(name);
	    return q->second;
	}
	++p;
    }
    return empty;
}

bool
DescriptorVariables::hasVariable(const string& name) const
{
    vector<VariableScope>::const_reverse_iterator p = _scopes.rbegin();
    while(p != _scopes.rend())
    {
	map<string, string>::const_iterator q = p->variables.find(name);
	if(q != p->variables.end())
	{
	    return true;
	}
	++p;
    }
    return false;
}

void
DescriptorVariables::addVariable(const string& name, const string& value)
{
    if(_scopes.back().parameters.find(name) != _scopes.back().parameters.end())
    {
	throw "can't define variable `" + name + "': a parameter with the same was previously defined";
    }
    if(_scopes.back().used.find(name) != _scopes.back().used.end())
    {
	throw "can't redefine variable `" + name + "' after its use";
    }
    _scopes.back().variables[name] = value;
}

void
DescriptorVariables::remove(const string& name)
{
    _scopes.back().variables.erase(name);
}

void
DescriptorVariables::reset(const map<string, string>& vars, const vector<string>& targets)
{
    _scopes.clear();
    push(vars);

    _deploymentTargets = targets;
}

void
DescriptorVariables::push(const map<string, string>& vars)
{
    VariableScope scope;
    if(!_scopes.empty())
    {
	scope.substitution = _scopes.back().substitution;
    }
    else
    {
	scope.substitution = true;
    }
    scope.variables = vars;
    _scopes.push_back(scope);
}

void
DescriptorVariables::push()
{
    push(map<string, string>());
}

void
DescriptorVariables::pop()
{
    _scopes.pop_back();
}

map<string, string>
DescriptorVariables::getCurrentScopeVariables() const
{
    return _scopes.back().variables;
}

vector<string>
DescriptorVariables::getCurrentScopeParameters() const
{
    return vector<string>(_scopes.back().parameters.begin(), _scopes.back().parameters.end());
}

void
DescriptorVariables::addParameter(const string& name)
{
    if(_scopes.back().variables.find(name) != _scopes.back().variables.end())
    {
	throw "can't declare parameter `" + name + "': a variable with the same was previously defined";
    }
    _scopes.back().parameters.insert(name);
}

vector<string>
DescriptorVariables::getDeploymentTargets(const string& prefix) const
{
    if(prefix.empty())
    {
	return _deploymentTargets;
    }

    vector<string> targets;
    for(vector<string>::const_iterator p = _deploymentTargets.begin(); p != _deploymentTargets.end(); ++p)
    {
	string::size_type pos = p->find(prefix);
	if(pos != string::npos)
	{
	    targets.push_back(p->substr(prefix.size()));
	}
    }
    return targets;
}

void
DescriptorVariables::substitution(bool substitution)
{
    _scopes.back().substitution = substitution;
}

bool
DescriptorVariables::substitution() const
{
    return _scopes.back().substitution;
}

string
DescriptorVariables::substituteImpl(const string& v, set<string>& missing)
{
    if(!substitution())
    {
	return v;
    }

    string value(v);
    string::size_type beg = 0;
    string::size_type end = 0;

    while((beg = value.find("${", beg)) != string::npos)
    {
	if(beg > 0 && value[beg - 1] == '$')
	{
	    string::size_type escape = beg - 1;
	    while(escape > 0 && value[escape - 1] == '$')
	    {
		--escape;
	    }
	    
	    value.replace(escape, beg - escape, (beg - escape) / 2, '$');
	    if((beg - escape) % 2)
	    {
		++beg;
		continue;
	    }
	    else
	    {
		beg -= (beg - escape) / 2;
	    }
	}

	end = value.find("}", beg);
	
	if(end == string::npos)
	{
	    throw "malformed variable name in the '" + value + "' value";
	}
	
	string name = value.substr(beg + 2, end - beg - 2);
	if(!hasVariable(name))
	{
	    missing.insert(name);
	    ++beg;
	    continue;	   
	}
	else
	{
	    string val = getVariable(name);
	    value.replace(beg, end - beg + 1, val);
	    beg += val.length();
	}
    }

    return value;
}

DescriptorTemplates::DescriptorTemplates(const ApplicationDescriptorPtr& descriptor) : _application(descriptor)
{
}

void
DescriptorTemplates::setDescriptor(const ApplicationDescriptorPtr& desc)
{
    _application = desc;
}

ServerDescriptorPtr
DescriptorTemplates::instantiateServer(const DescriptorHelper& helper, 
				       const string& name, 
				       const map<string, string>& parameters)
{
    TemplateDescriptorDict::const_iterator p = _application->serverTemplates.find(name);
    if(p == _application->serverTemplates.end())
    {
	throw "unknown template `" + name + "'";
    }
    
    set<string> missing;
    Substitute substitute(helper.getVariables(), missing);
    map<string, string> params = parameters;

    set<string> unknown;
    for(map<string, string>::iterator q = params.begin(); q != params.end(); ++q)
    {
	if(find(p->second.parameters.begin(), p->second.parameters.end(), q->first) == p->second.parameters.end())
	{
	    unknown.insert(q->first);
	}
	substitute(q->second);
    }
    if(!unknown.empty())
    {
	ostringstream os;
	os << "server template instance unknown parameters: ";
	copy(unknown.begin(), unknown.end(), ostream_iterator<string>(os, " "));
	throw os.str();
    }

    set<string> missingParams;
    for(vector<string>::const_iterator q = p->second.parameters.begin(); q != p->second.parameters.end(); ++q)
    {
	if(params.find(*q) == params.end())
	{
	    missingParams.insert(*q);
	}
    }
    if(!missingParams.empty())
    {
	ostringstream os;
	os << "server template instance undefined parameters: ";
	copy(missingParams.begin(), missingParams.end(), ostream_iterator<string>(os, " "));
	throw os.str();
    }
    
    helper.getVariables()->push(params);
    ServerDescriptorPtr tmpl = ServerDescriptorPtr::dynamicCast(p->second.descriptor);
    assert(tmpl);
    ServerDescriptorPtr descriptor = ServerDescriptorHelper(helper, tmpl).instantiate(missing);
    helper.getVariables()->pop();
    if(!missing.empty())
    {
	ostringstream os;
	os << "server template instance undefined variables: ";
	copy(missing.begin(), missing.end(), ostream_iterator<string>(os, " "));
	throw os.str();   
    }

    return descriptor;
}

ServiceDescriptorPtr
DescriptorTemplates::instantiateService(const DescriptorHelper& helper, const string& name, 
					const map<string, string>& parameters)
{
    TemplateDescriptorDict::const_iterator p = _application->serviceTemplates.find(name);
    if(p == _application->serviceTemplates.end())
    {
	throw "unknown template `" + name + "'";
    }
    
    set<string> missing;
    Substitute substitute(helper.getVariables(), missing);
    map<string, string> params = parameters;

    set<string> unknown;
    for(map<string, string>::iterator q = params.begin(); q != params.end(); ++q)
    {
	if(find(p->second.parameters.begin(), p->second.parameters.end(), q->first) == p->second.parameters.end())
	{
	    unknown.insert(q->first);
	}
	substitute(q->second);
    }
    if(!unknown.empty())
    {
	ostringstream os;
	os << "service template instance unknown parameters: ";
	copy(unknown.begin(), unknown.end(), ostream_iterator<string>(os, " "));
	throw os.str();
    }

    set<string> missingParams;
    for(vector<string>::const_iterator q = p->second.parameters.begin(); q != p->second.parameters.end(); ++q)
    {
	if(params.find(*q) == params.end())
	{
	    missingParams.insert(*q);
	}
    }
    if(!missingParams.empty())
    {
	ostringstream os;
	os << "service template instance undefined parameters: ";
	copy(missingParams.begin(), missingParams.end(), ostream_iterator<string>(os, " "));
	throw os.str();
    }
    for(map<string, string>::iterator q = params.begin(); q != params.end(); ++q)
    {
	substitute(q->second);
    }

    for(vector<string>::const_iterator q = p->second.parameters.begin(); q != p->second.parameters.end(); ++q)
    {
	if(params.find(*q) == params.end())
	{
	    missing.insert(*q);
	}
    }
    if(!missing.empty())
    {
	ostringstream os;
	os << "service template instance undefined parameters: ";
	copy(missing.begin(), missing.end(), ostream_iterator<string>(os, " "));
	throw os.str();
    }
    
    helper.getVariables()->push(params);
    ServiceDescriptorPtr tmpl = ServiceDescriptorPtr::dynamicCast(p->second.descriptor);
    assert(tmpl);
    ServiceDescriptorPtr descriptor = ServiceDescriptorHelper(helper, tmpl).instantiate(missing);
    helper.getVariables()->pop();

    if(!missing.empty())
    {
	ostringstream os;
	os << "service template instance undefined variables: ";
	copy(missing.begin(), missing.end(), ostream_iterator<string>(os, " "));
	throw os.str();   
    }

    return descriptor;
}

void
DescriptorTemplates::addServerTemplate(const string& id, const ServerDescriptorPtr& desc, const Ice::StringSeq& vars)
{
    //
    // Add the template to the application.
    //
    TemplateDescriptor tmpl;
    tmpl.descriptor = desc;
    tmpl.parameters = vars;
    _application->serverTemplates.insert(make_pair(id, tmpl));
}

void
DescriptorTemplates::addServiceTemplate(const string& id, const ServiceDescriptorPtr& desc, const Ice::StringSeq& vars)
{
    //
    // Add the template to the application.
    //
    TemplateDescriptor tmpl;
    tmpl.descriptor = desc;
    tmpl.parameters = vars;
    _application->serviceTemplates.insert(make_pair(id, tmpl));
}

ApplicationDescriptorPtr
DescriptorTemplates::getApplicationDescriptor() const
{
    return _application;
}

XmlAttributesHelper::XmlAttributesHelper(const DescriptorVariablesPtr& variables, const IceXML::Attributes& attrs) :
	_variables(variables), _attributes(attrs)
{
}

bool
XmlAttributesHelper::contains(const string& name)
{
    return _attributes.find(name) != _attributes.end();
}

int
XmlAttributesHelper::asInt(const string& name, const string& def)
{
    int value;
    istringstream v(operator()(name, def));
    if(!(v >> value) || !v.eof())
    {
	return 0;
    }    
    return value;
}

string 
XmlAttributesHelper::operator()(const string& name)
{
    IceXML::Attributes::const_iterator p = _attributes.find(name);
    if(p == _attributes.end())
    {
	throw "missing attribute '" + name + "'";
    }
    string v = _variables->substitute(p->second);
    if(v.empty())
    {
	throw "attribute '" + name + "' is empty";
    }
    return v;
}

string
XmlAttributesHelper::operator()(const string& name, const string& def)
{
    IceXML::Attributes::const_iterator p = _attributes.find(name);
    if(p == _attributes.end())
    {
	return _variables->substitute(def);
    }
    else
    {
	return _variables->substitute(p->second);
    }
}

DescriptorHelper::DescriptorHelper(const Ice::CommunicatorPtr& communicator, 
				   const DescriptorVariablesPtr& vars,
				   const DescriptorTemplatesPtr& templates) :
    _communicator(communicator),
    _variables(vars),
    _templates(templates)
{
}

DescriptorHelper::DescriptorHelper(const DescriptorHelper& desc) :
    _communicator(desc._communicator),
    _variables(desc._variables),
    _templates(desc._templates)
{
}

DescriptorHelper::~DescriptorHelper()
{
}

const DescriptorVariablesPtr&
DescriptorHelper::getVariables() const
{
    return _variables;
}

ApplicationDescriptorHelper::ApplicationDescriptorHelper(const Ice::CommunicatorPtr& communicator,
							 const ApplicationDescriptorPtr& descriptor) :
    DescriptorHelper(communicator, new DescriptorVariables(), new DescriptorTemplates(descriptor)),
    _descriptor(descriptor)
{
    _variables->push(descriptor->variables);
    _variables->addVariable("application", _descriptor->name);
}

ApplicationDescriptorHelper::ApplicationDescriptorHelper(const Ice::CommunicatorPtr& communicator,
							 const DescriptorVariablesPtr& variables,
							 const IceXML::Attributes& attrs) :
    DescriptorHelper(communicator, variables, new DescriptorTemplates(new ApplicationDescriptor())),
    _descriptor(_templates->getApplicationDescriptor())
{
    XmlAttributesHelper attributes(_variables, attrs);
    _descriptor->name = attributes("name");
    _descriptor->targets = _variables->getDeploymentTargets("");
    _variables->addVariable("application", _descriptor->name);
}

void
ApplicationDescriptorHelper::endParsing()
{
    _descriptor->variables = _variables->getCurrentScopeVariables();
}

const ApplicationDescriptorPtr&
ApplicationDescriptorHelper::getDescriptor() const
{
    return _descriptor;
}

void
ApplicationDescriptorHelper::setComment(const string& comment)
{
    _descriptor->comment = comment;
}

void
ApplicationDescriptorHelper::addNode(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);
    
    string node = attributes("name");	
    _variables->push();
    _variables->addVariable("node", node);
    for(NodeDescriptorSeq::const_iterator p = _descriptor->nodes.begin(); p != _descriptor->nodes.end(); ++p)
    {
	if(p->name == node)
	{
	    _variables->push(p->variables);
	    break;
	}
    }
}

void
ApplicationDescriptorHelper::endNodeParsing()
{
    NodeDescriptor node;
    node.name = _variables->getVariable("node");
    node.variables = _variables->getCurrentScopeVariables();
    _descriptor->nodes.push_back(node);
    _variables->pop();
}


void
ApplicationDescriptorHelper::addServer(const string& tmpl, const IceXML::Attributes& attrs)
{
    assert(_variables->hasVariable("node"));
    ServerInstanceDescriptor instance;
    instance._cpp_template = tmpl;
    instance.node = _variables->getVariable("node");
    instance.parameterValues = attrs;
    instance.parameterValues.erase("template");
    _descriptor->servers.push_back(instantiate(instance));
}

void
ApplicationDescriptorHelper::addServer(const ServerDescriptorPtr& descriptor)
{
    assert(_variables->hasVariable("node"));
    ServerInstanceDescriptor instance;
    instance.node = _variables->getVariable("node");
    instance.descriptor = descriptor;
    instance.targets = _variables->getDeploymentTargets(descriptor->name + ".");
    _descriptor->servers.push_back(instance);
}

auto_ptr<ServerDescriptorHelper>
ApplicationDescriptorHelper::addServerTemplate(const std::string& id, const IceXML::Attributes& attrs)
{
    return auto_ptr<ServerDescriptorHelper>(new ServerDescriptorHelper(*this, attrs, id));
}

auto_ptr<ServiceDescriptorHelper>
ApplicationDescriptorHelper::addServiceTemplate(const std::string& id, const IceXML::Attributes& attrs)
{
    return auto_ptr<ServiceDescriptorHelper>(new ServiceDescriptorHelper(*this, attrs, id));
}

void
ApplicationDescriptorHelper::addReplicatedAdapter(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);

    ReplicatedAdapterDescriptor adapter;
    adapter.id = attributes("id");
    string policy = attributes("load-balancing", "random");
    if(policy == "random")
    {
	adapter.loadBalancing = Random;
    }
    else if(policy == "round-robin")
    {
	adapter.loadBalancing = RoundRobin;
    }
    else if(policy == "adaptive")
    {
	adapter.loadBalancing = Adaptive;
    }
    _descriptor->replicatedAdapters.push_back(adapter);
}

ApplicationUpdateDescriptor
ApplicationDescriptorHelper::update(const ApplicationUpdateDescriptor& update)
{
    ApplicationUpdateDescriptor newUpdate = update;
    ApplicationDescriptorPtr newApp = new ApplicationDescriptor();
    ApplicationDescriptorPtr oldApp = _descriptor;
    _descriptor = newApp;
    _templates->setDescriptor(newApp);

    newApp->name = oldApp->name;
    newApp->comment = newUpdate.comment ? newUpdate.comment->value : oldApp->comment;
    newApp->targets = oldApp->targets;

    Ice::StringSeq::const_iterator p;

    //
    // Copy the updated replicated adapters in the new descriptor and
    // add back the old adapters which weren't removed.
    //
    newApp->replicatedAdapters = updateSeqElts(oldApp->replicatedAdapters, newUpdate.replicatedAdapters,
					       newUpdate.removeReplicatedAdapters, GetReplicatedAdapterId());

    //
    // Copy the old variables in the new descriptor, remove the
    // required variables and add the updated ones.
    //
    newApp->variables = oldApp->variables;
    for(p = newUpdate.removeVariables.begin(); p != newUpdate.removeVariables.end(); ++p)
    {
	newApp->variables.erase(*p);
	_variables->remove(*p);
    }
    for(map<string, string>::const_iterator q = newUpdate.variables.begin(); q != newUpdate.variables.end(); ++q)
    {
	newApp->variables[q->first] = q->second;
	_variables->addVariable(q->first, q->second);
    }

    newApp->serverTemplates = 
	updateDictElts(oldApp->serverTemplates, newUpdate.serverTemplates, newUpdate.removeServerTemplates);
    newApp->serviceTemplates = 
	updateDictElts(oldApp->serviceTemplates, newUpdate.serviceTemplates, newUpdate.removeServiceTemplates);
    newApp->nodes = updateSeqElts(oldApp->nodes, newUpdate.nodes, newUpdate.removeNodes, GetNodeName());

    //
    // Copy the updated servers in the new descriptor and add back
    // the old servers which weren't removed.
    //
    // NOTE: we also re-instantiate the old servers since their
    // template might have changed.
    //
    newApp->servers.clear();
    for(ServerInstanceDescriptorSeq::iterator q = newUpdate.servers.begin(); q != newUpdate.servers.end(); ++q)
    {
	*q = instantiate(*q);
	newApp->servers.push_back(*q);
    }

    set<string> removed(newUpdate.removeServers.begin(), newUpdate.removeServers.end());
    set<string> updated;
    for_each(newApp->servers.begin(), newApp->servers.end(), AddServerName(updated));
    for(ServerInstanceDescriptorSeq::const_iterator q = oldApp->servers.begin(); q != oldApp->servers.end(); ++q)
    {
	ServerInstanceDescriptor inst = instantiate(*q); // Re-instantiate old servers.
	if(updated.find(inst.descriptor->name) == updated.end() && removed.find(inst.descriptor->name) == removed.end())
	{
	    if(q->node != inst.node ||
	       ServerDescriptorHelper(*this, q->descriptor) != ServerDescriptorHelper(*this, inst.descriptor))
	    {
		newUpdate.servers.push_back(inst);
	    }
	    newApp->servers.push_back(inst);
	}
    }

    return newUpdate;
}

ApplicationUpdateDescriptor
ApplicationDescriptorHelper::diff(const ApplicationDescriptorPtr& orig)
{
    ApplicationUpdateDescriptor update;
    update.comment = _descriptor->comment != orig->comment ? new BoxedComment(_descriptor->comment) : BoxedCommentPtr();
    update.targets = _descriptor->targets != orig->targets ? new BoxedTargets(_descriptor->targets) : BoxedTargetsPtr();

    update.variables = getDictUpdatedElts(orig->variables, _descriptor->variables);
    update.removeVariables = getDictRemovedElts(orig->variables, _descriptor->variables);

    GetReplicatedAdapterId rk;
    update.replicatedAdapters = getSeqUpdatedElts(orig->replicatedAdapters, _descriptor->replicatedAdapters, rk);
    update.removeReplicatedAdapters = getSeqRemovedElts(orig->replicatedAdapters, _descriptor->replicatedAdapters, rk);

    TemplateDescriptorEqual tmpleq(*this);
    update.serverTemplates = getDictUpdatedElts(orig->serverTemplates, _descriptor->serverTemplates, tmpleq);
    update.removeServerTemplates = getDictRemovedElts(orig->serverTemplates, _descriptor->serverTemplates);
    update.serviceTemplates = getDictUpdatedElts(orig->serviceTemplates, _descriptor->serviceTemplates, tmpleq);
    update.removeServiceTemplates = getDictRemovedElts(orig->serviceTemplates, _descriptor->serviceTemplates);

    GetNodeName nk;
    update.nodes = getSeqUpdatedElts(orig->nodes, _descriptor->nodes, nk);
    update.removeNodes = getSeqRemovedElts(orig->nodes, _descriptor->nodes, nk);

    GetServerName sn;
    ServerInstanceDescriptorEqual svreq(*this);
    update.servers = getSeqUpdatedElts(orig->servers, _descriptor->servers, sn, svreq);
    update.removeServers = getSeqRemovedElts(orig->servers, _descriptor->servers, sn);

    return update;
}

void
ApplicationDescriptorHelper::addServerInstance(const string& tmpl, 
					       const string& node, 
					       const map<string, string>& parameters)
{
    pushNodeVariables(node);

    ServerInstanceDescriptor instance;
    instance._cpp_template = tmpl;
    instance.node = node;
    instance.parameterValues = parameters;
    instance.descriptor = _templates->instantiateServer(*this, tmpl, instance.parameterValues);
    _descriptor->servers.push_back(instance);    

    _variables->pop();
}

void
ApplicationDescriptorHelper::instantiate()
{
    for(ServerInstanceDescriptorSeq::iterator p = _descriptor->servers.begin(); p != _descriptor->servers.end(); ++p)
    {
	*p = instantiate(*p);
    }
}

ServerInstanceDescriptor
ApplicationDescriptorHelper::instantiate(const ServerInstanceDescriptor& inst)
{
    ServerInstanceDescriptor instance = inst;
    pushNodeVariables(inst.node);
    if(instance._cpp_template.empty())
    {
	//
	// We can't re-instantiate here -- this would break escaped variables.
	//
// 	assert(instance.descriptor);
// 	set<string> missing;
// 	instance.descriptor = ServerDescriptorHelper(*this, instance.descriptor).instantiate(missing);
// 	if(!missing.empty())
// 	{
// 	    ostringstream os;
// 	    os << "server undefined variables: ";
// 	    copy(missing.begin(), missing.end(), ostream_iterator<string>(os, " "));
// 	    throw os.str();
// 	}
    }
    else
    {
	instance.descriptor = _templates->instantiateServer(*this, instance._cpp_template, instance.parameterValues);
    }
    _variables->pop();
    return instance;
}

void
ApplicationDescriptorHelper::pushNodeVariables(const string& node)
{
    NodeDescriptorSeq::const_iterator q;
    for(q = _descriptor->nodes.begin(); q != _descriptor->nodes.end(); ++q)
    {
	if(q->name == node)
	{
	    _variables->push(q->variables);
	    break;
	}
    }
    if(q == _descriptor->nodes.end())
    {
	_variables->push();
    }
    _variables->addVariable("node", node);
}

ComponentDescriptorHelper::ComponentDescriptorHelper(const DescriptorHelper& helper) : DescriptorHelper(helper)
{
}

ComponentDescriptorHelper::ComponentDescriptorHelper(const Ice::CommunicatorPtr& communicator, 
						     const DescriptorVariablesPtr& vars,
						     const DescriptorTemplatesPtr& templates) :
    DescriptorHelper(communicator, vars, templates)
{
}

void
ComponentDescriptorHelper::init(const ComponentDescriptorPtr& descriptor, const IceXML::Attributes& attrs)
{
    _descriptor = descriptor;

    if(attrs.empty())
    {
	return;
    }

    XmlAttributesHelper attributes(_variables, attrs);
    _descriptor->name = attributes("name");
}

bool
ComponentDescriptorHelper::operator==(const ComponentDescriptorHelper& helper) const
{
    if(_descriptor->ice_id() != helper._descriptor->ice_id())
    {
	return false;
    }

    if(_descriptor->name != helper._descriptor->name)
    {
	return false;
    }

    if(_descriptor->comment != helper._descriptor->comment)
    {
	return false;
    }

    if(set<AdapterDescriptor>(_descriptor->adapters.begin(), _descriptor->adapters.end())  != 
       set<AdapterDescriptor>(helper._descriptor->adapters.begin(), helper._descriptor->adapters.end()))
    {
	return false;
    }

    if(set<PropertyDescriptor>(_descriptor->properties.begin(), _descriptor->properties.end()) != 
       set<PropertyDescriptor>(helper._descriptor->properties.begin(), helper._descriptor->properties.end()))
    {
	return false;
    }

    if(set<DbEnvDescriptor>(_descriptor->dbEnvs.begin(), _descriptor->dbEnvs.end()) != 
       set<DbEnvDescriptor>(helper._descriptor->dbEnvs.begin(), helper._descriptor->dbEnvs.end()))
    {
	return false;
    }

    if(_descriptor->variables != helper._descriptor->variables)
    {
	return false;
    }

    return true;
}

bool
ComponentDescriptorHelper::operator!=(const ComponentDescriptorHelper& helper) const
{
    return !operator==(helper);
}

void
ComponentDescriptorHelper::setComment(const string& comment)
{
    _descriptor->comment = comment;
}

void
ComponentDescriptorHelper::addProperty(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);
    PropertyDescriptor prop;
    prop.name = attributes("name");
    prop.value = attributes("value", "");
    _descriptor->properties.push_back(prop);
}

void
ComponentDescriptorHelper::addAdapter(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);
    
    AdapterDescriptor desc;
    desc.name = attributes("name");
    desc.id = attributes("id", "");
    if(desc.id.empty())
    {
	string fqn = "${server}";
	if(ServiceDescriptorPtr::dynamicCast(_descriptor))
	{
	    fqn += ".${service}";
	}
	desc.id = _variables->substitute(fqn) + "." + desc.name;
    }
    desc.endpoints = attributes("endpoints");
    desc.registerProcess = attributes("register", "false") == "true";
    desc.noWaitForActivation = attributes("no-wait-for-activation", "false") == "true";
    _descriptor->adapters.push_back(desc);
}

void
ComponentDescriptorHelper::addObject(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);

    ObjectDescriptor object;
    object.type = attributes("type", "");
    object.id = Ice::stringToIdentity(attributes("identity"));
    _descriptor->adapters.back().objects.push_back(object);
}

void
ComponentDescriptorHelper::addDbEnv(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);

    DbEnvDescriptor desc;
    desc.name = attributes("name");


    DbEnvDescriptorSeq::iterator p;
    for(p = _descriptor->dbEnvs.begin(); p != _descriptor->dbEnvs.end(); ++p)
    {
	//
	// We are re-opening the dbenv element to define more properties.
	//
	if(p->name == desc.name)
	{	
	    break;
	}
    }

    if(p != _descriptor->dbEnvs.end())
    {
	//
	// Remove the previously defined dbenv, we'll add it back again when 
	// the dbenv element end tag is reached.
	//
	desc = *p;
	_descriptor->dbEnvs.erase(p);
    }	

    if(desc.dbHome.empty())
    {
	desc.dbHome = attributes("home", "");
    }
    
    _descriptor->dbEnvs.push_back(desc);
}

void
ComponentDescriptorHelper::addDbEnvProperty(const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);

    PropertyDescriptor prop;
    prop.name = attributes("name");
    prop.value = attributes("value", "");
    _descriptor->dbEnvs.back().properties.push_back(prop);
}

void
ComponentDescriptorHelper::instantiateImpl(const ComponentDescriptorPtr& desc, set<string>& missing) const
{
    Substitute substitute(_variables, missing);
    substitute(desc->name);
    substitute(desc->comment);
    for(AdapterDescriptorSeq::iterator p = desc->adapters.begin(); p != desc->adapters.end(); ++p)
    {
	substitute(p->name);
	substitute(p->id);
	substitute(p->endpoints);
	for(ObjectDescriptorSeq::iterator q = p->objects.begin(); q != p->objects.end(); ++q)
	{
	    substitute(q->type);
	    substitute(q->id.name);
	    substitute(q->id.category);
	}
    }
    for(PropertyDescriptorSeq::iterator p = desc->properties.begin(); p != desc->properties.end(); ++p)
    {
	substitute(p->name);
	substitute(p->value);	
    }
    for(DbEnvDescriptorSeq::iterator p = desc->dbEnvs.begin(); p != desc->dbEnvs.end(); ++p)
    {
	substitute(p->name);
	substitute(p->dbHome);	
	for(PropertyDescriptorSeq::iterator q = p->properties.begin(); q != p->properties.end(); ++q)
	{
	    substitute(q->name);
	    substitute(q->value);	
	}
    }
}

ServerDescriptorHelper::ServerDescriptorHelper(const DescriptorHelper& helper, const ServerDescriptorPtr& descriptor) :
    ComponentDescriptorHelper(helper),
    _descriptor(descriptor)
{
    ComponentDescriptorHelper::init(_descriptor);
    _variables->push(_descriptor->variables);
}

ServerDescriptorHelper::ServerDescriptorHelper(const DescriptorHelper& helper, const IceXML::Attributes& attrs,
					       const string& id) :
    ComponentDescriptorHelper(helper),
    _templateId(id)
{
    XmlAttributesHelper attributes(_variables, attrs);

    _variables->push();
    if(!_templateId.empty())
    {
	_variables->substitution(false);
    }

    string interpreter = attributes("interpreter", "");
    if(interpreter == "icebox" || interpreter == "java-icebox")
    {
	_descriptor = new IceBoxDescriptor();
	_descriptor->exe = attributes("exe", "");
    }
    else
    {
	_descriptor = new ServerDescriptor();
	_descriptor->exe = attributes("exe");
    }
    _descriptor->interpreter = interpreter;
    _descriptor->activationTimeout = attributes.asInt("activation-timeout", "0");
    _descriptor->deactivationTimeout = attributes.asInt("deactivation-timeout", "0");

    ComponentDescriptorHelper::init(_descriptor, attrs);

    _descriptor->pwd = attributes("pwd", "");
    _descriptor->activation = attributes("activation", "manual");
    
    if(interpreter == "icebox" || interpreter == "java-icebox")
    {
	IceBoxDescriptorPtr iceBox = IceBoxDescriptorPtr::dynamicCast(_descriptor);
	iceBox->endpoints = attributes("endpoints");
	
	PropertyDescriptor prop;
	prop.name = "IceBox.ServiceManager.Identity";
	prop.value = _descriptor->name + "/ServiceManager";
	_descriptor->properties.push_back(prop);
	
	AdapterDescriptor adapter;
	adapter.name = "IceBox.ServiceManager";
	adapter.endpoints = attributes("endpoints");
	adapter.id = _descriptor->name + "." + adapter.name;
	adapter.registerProcess = true;
	adapter.noWaitForActivation = false;
	_descriptor->adapters.push_back(adapter);
    }

    if(_templateId.empty())
    {
	_variables->addVariable("server", _descriptor->name);
    }
}

ServerDescriptorHelper::~ServerDescriptorHelper()
{
    _variables->pop();    
}

void
ServerDescriptorHelper::endParsing()
{
    if(!_templateId.empty())
    {
	_descriptor->variables = _variables->getCurrentScopeVariables();
	_templates->addServerTemplate(_templateId, _descriptor, _variables->getCurrentScopeParameters());
	_variables->substitution(true);
    }
}

bool
ServerDescriptorHelper::operator==(const ServerDescriptorHelper& helper) const
{
    if(!ComponentDescriptorHelper::operator==(helper))
    {
	return false;
    }

    if(_descriptor->exe != helper._descriptor->exe)
    {
	return false;
    }

    if(_descriptor->pwd != helper._descriptor->pwd)
    {
	return false;
    }

    if(_descriptor->activation != helper._descriptor->activation)
    {
	return false;
    }

    if(_descriptor->activationTimeout != helper._descriptor->activationTimeout)
    {
	return false;
    }

    if(_descriptor->deactivationTimeout != helper._descriptor->deactivationTimeout)
    {
	return false;
    }

    if(set<string>(_descriptor->options.begin(), _descriptor->options.end()) != 
       set<string>(helper._descriptor->options.begin(), helper._descriptor->options.end()))
    {
	return false;
    }

    if(set<string>(_descriptor->envs.begin(), _descriptor->envs.end()) != 
       set<string>(helper._descriptor->envs.begin(), helper._descriptor->envs.end()))
    {
	return false;
    }
    
    if(_descriptor->interpreter != helper._descriptor->interpreter)
    {
	return false;
    }

    if(set<string>(_descriptor->interpreterOptions.begin(), _descriptor->interpreterOptions.end()) != 
       set<string>(helper._descriptor->interpreterOptions.begin(), helper._descriptor->interpreterOptions.end()))
    {
	return false;
    }

    if(IceBoxDescriptorPtr::dynamicCast(_descriptor))
    {
	IceBoxDescriptorPtr ilhs = IceBoxDescriptorPtr::dynamicCast(_descriptor);
	IceBoxDescriptorPtr irhs = IceBoxDescriptorPtr::dynamicCast(helper._descriptor);
	if(!irhs)
	{
	    return false;
	}

	if(ilhs->endpoints != irhs->endpoints)
	{
	    return false;
	}
	
	if(ilhs->services.size() != irhs->services.size())
	{
	    return false;
	}
	
	//
	// First we compare the service instances which have a
	// descriptor set (this is the case for services not based on
	// a template or server instances).
	//
	for(ServiceInstanceDescriptorSeq::const_iterator p = ilhs->services.begin(); p != ilhs->services.end(); ++p)
	{
	    if(p->descriptor)
	    {
		bool found = false;
		for(ServiceInstanceDescriptorSeq::const_iterator q = irhs->services.begin();
		    q != irhs->services.end();
		    ++q)
		{
		    if(q->descriptor && p->descriptor->name == q->descriptor->name)
		    {
			found = true;
			if(ServiceDescriptorHelper(*this, p->descriptor) !=
			   ServiceDescriptorHelper(*this, q->descriptor))
			{
			    return false;
			}
			break;
		    }
		}
		if(!found)
		{
		    return false;
		}
	    }
	}

	//
	// Then, we compare the service instances for which no
	// descriptor is set.
	//
	set<ServiceInstanceDescriptor> lsvcs;
	set<ServiceInstanceDescriptor> rsvcs;
	for(ServiceInstanceDescriptorSeq::const_iterator p = ilhs->services.begin(); p != ilhs->services.end(); ++p)
	{
	    if(!p->descriptor)
	    {
		lsvcs.insert(*p);
	    }
	}
	for(ServiceInstanceDescriptorSeq::const_iterator p = irhs->services.begin(); p != irhs->services.end(); ++p)
	{
	    if(!p->descriptor)
	    {
		rsvcs.insert(*p);
	    }
	}
	if(lsvcs != rsvcs)
	{
	    return false;
	}
    }
    return true;
}

bool
ServerDescriptorHelper::operator!=(const ServerDescriptorHelper& helper) const
{
    return !operator==(helper);
}

const ServerDescriptorPtr&
ServerDescriptorHelper::getDescriptor() const
{
    return _descriptor;
}

const string&
ServerDescriptorHelper::getTemplateId() const
{
    return _templateId;
}

auto_ptr<ServiceDescriptorHelper>
ServerDescriptorHelper::addServiceTemplate(const std::string& id, const IceXML::Attributes& attrs)
{
    return auto_ptr<ServiceDescriptorHelper>(new ServiceDescriptorHelper(*this, attrs, id));
}

void
ServerDescriptorHelper::addService(const string& tmpl, const IceXML::Attributes& attrs)
{
    XmlAttributesHelper attributes(_variables, attrs);
    IceBoxDescriptorPtr iceBox = IceBoxDescriptorPtr::dynamicCast(_descriptor);
    if(!iceBox)
    {
	throw "element <service> can only be a child of an IceBox <server> element";
    }

    ServiceInstanceDescriptor instance;
    instance._cpp_template = tmpl;
    instance.parameterValues = attrs;
    instance.parameterValues.erase("template");
    instance.descriptor = _templates->instantiateService(*this, instance._cpp_template, instance.parameterValues);
    iceBox->services.push_back(instance);
}

void
ServerDescriptorHelper::addService(const ServiceDescriptorPtr& descriptor)
{
    IceBoxDescriptorPtr iceBox = IceBoxDescriptorPtr::dynamicCast(_descriptor);
    if(!iceBox)
    {
	throw "element <service> can only be a child of an IceBox <server> element";
    }

    ServiceInstanceDescriptor instance;
    instance.descriptor = descriptor;
    if(_templateId.empty())
    {
	instance.targets = _variables->getDeploymentTargets(_descriptor->name + "." + descriptor->name + ".");
    }
    iceBox->services.push_back(instance);
}

void
ServerDescriptorHelper::addOption(const string& option)
{
    _descriptor->options.push_back(option);
}

void
ServerDescriptorHelper::addEnv(const string& env)
{
    _descriptor->envs.push_back(env);
}

void
ServerDescriptorHelper::addInterpreterOption(const string& option)
{
    if(_descriptor->interpreter.empty())
    {
	throw "element <interpreter-option> can only be specified if the interpreter attribute of the <server> "
	    "element is not empty";
    }

    _descriptor->interpreterOptions.push_back(option);
}

ServerDescriptorPtr
ServerDescriptorHelper::instantiate(set<string>& missing) const
{
    ServerDescriptorPtr desc = ServerDescriptorPtr::dynamicCast(_descriptor->ice_clone());
    instantiateImpl(desc, missing);
    return desc;    
}

void
ServerDescriptorHelper::instantiateImpl(const ServerDescriptorPtr& desc, set<string>& missing) const
{
    Substitute substitute(_variables, missing);
    substitute(desc->name);
    _variables->addVariable("server", desc->name);

    ComponentDescriptorHelper::instantiateImpl(desc, missing);

    substitute(desc->exe);
    substitute(desc->pwd);
    for_each(desc->options.begin(), desc->options.end(), substitute);
    for_each(desc->envs.begin(), desc->envs.end(), substitute);
    substitute(desc->interpreter);
    for_each(desc->interpreterOptions.begin(), desc->interpreterOptions.end(), substitute);

    IceBoxDescriptorPtr iceBox = IceBoxDescriptorPtr::dynamicCast(desc);
    if(iceBox)
    {
 	for(ServiceInstanceDescriptorSeq::iterator p = iceBox->services.begin(); p != iceBox->services.end(); ++p)
 	{
	    if(p->_cpp_template.empty())
	    {
		ServiceDescriptorPtr service = p->descriptor;
		assert(service);
		p->descriptor = ServiceDescriptorHelper(*this, service).instantiate(missing);
	    }
	    else
	    {
		p->descriptor = _templates->instantiateService(*this, p->_cpp_template, p->parameterValues);
	    }
 	}
    }
}

ServiceDescriptorHelper::ServiceDescriptorHelper(const DescriptorHelper& helper, const ServiceDescriptorPtr& desc) :
    ComponentDescriptorHelper(helper),
    _descriptor(desc)
{
    init(_descriptor);
    _variables->push(_descriptor->variables);
}

ServiceDescriptorHelper::ServiceDescriptorHelper(const DescriptorHelper& helper, const IceXML::Attributes& attrs,
						 const string& id) :
    ComponentDescriptorHelper(helper),
    _descriptor(new ServiceDescriptor()),
    _templateId(id)
{
    XmlAttributesHelper attributes(_variables, attrs);

    _variables->push();
    if(!_templateId.empty())
    {
	_variables->substitution(false);
    }

    init(_descriptor, attrs);

    _descriptor->entry = attributes("entry");
    if(_templateId.empty())
    {
	_variables->addVariable("service", _descriptor->name);
    }
}

ServiceDescriptorHelper::~ServiceDescriptorHelper()
{
    _variables->pop();
}

void
ServiceDescriptorHelper::endParsing()
{
    if(!_templateId.empty())
    {
	_descriptor->variables = _variables->getCurrentScopeVariables();
	_templates->addServiceTemplate(_templateId, _descriptor, _variables->getCurrentScopeParameters());
	_variables->substitution(true);
    }
}

bool
ServiceDescriptorHelper::operator==(const ServiceDescriptorHelper& helper) const
{
    if(!ComponentDescriptorHelper::operator==(helper))
    {
	return false;
    }

    if(_descriptor->entry != helper._descriptor->entry)
    {
	return false;
    }

    return true;
}

bool
ServiceDescriptorHelper::operator!=(const ServiceDescriptorHelper& helper) const
{
    return !operator==(helper);
}

ServiceDescriptorPtr
ServiceDescriptorHelper::instantiate(set<string>& missing) const
{
    ServiceDescriptorPtr desc = ServiceDescriptorPtr::dynamicCast(_descriptor->ice_clone());
    instantiateImpl(desc, missing);
    return desc;    
}

const ServiceDescriptorPtr&
ServiceDescriptorHelper::getDescriptor() const
{
    return _descriptor;
}

const string&
ServiceDescriptorHelper::getTemplateId() const
{
    return _templateId;
}

void
ServiceDescriptorHelper::instantiateImpl(const ServiceDescriptorPtr& desc, set<string>& missing) const
{
    Substitute substitute(_variables, missing);
    substitute(desc->name);
    _variables->addVariable("service", desc->name);

    ComponentDescriptorHelper::instantiateImpl(desc, missing);

    substitute(desc->entry);
}
