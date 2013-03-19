#include <string>
#include <vector>
#include<list>

using namespace std;

class ProjectManager;
class PdbManager;
class FamilyManager; 
class BundleManager; 

class ICommand
{
public:	
	virtual string get_Name() = 0;
	virtual string set_Name(const string& name) = 0;
	virtual string Arguments() = 0;
	virtual int Run(const vector<string>& args) = 0;
};

class ProjectCommandBase
{
private:
    string projectPath;
protected:
    ProjectManager * projectManager;
    PdbManager * pdbManager;
    FamilyManager * familyManager;
    BundleManager * bundleManager;

    virtual bool ParseCommandLine(const vector<string>& args);

    bool CreateManagers(bool scanPdbs = true);
};

class AddCommand : protected ProjectCommandBase, public ICommand
    {
    private:
        list<string> patchedFilesToAdd;
        list<string> patchedComponentsToAdd;

    public:
        string Name;
        string Arguments();
        
        int Run(IEnumerable<string> args)
        

    protected:
        bool ParseCommandLine(const vector<string>& args)
        

        private IEnumerable<MsiResource> GetMsiResourcesForPatchProject(WixProject project)
        {
            HashSet<MsiResource> resources = new HashSet<MsiResource>();

            // pick out only components being added in this session
            IEnumerable<string> newComponents = project.NewComponentsBeingPatchedThisSession;

            UX.Trace("Looking for resources in project {0}({1}): {2}", DevDiv.ShortenEnlistmentPath(project.ProjectPath), project.ProjectType.ToString(), newComponents.Listify());

            bool unknownComponents = this.GetMsiResourcesForPatchedComponents(newComponents, resources);
            if (unknownComponents)
            {
                UX.Error("Unresolved resources. Typo? Bad remote store? Either way...#fail");
                return Enumerable.Empty<MsiResource>();
            }

            // Remove MSI resources if they're in packages serviced by major upgrades or non-shipping packages.
            List<MsiResource> ineligibleResources = new List<MsiResource>(resources.Where(resource => WixProject.IsInvalidProject(resource.Sku.WixprojPath, this.projectManager.AllInclusivePatch)));
            resources.ExceptWith(ineligibleResources);

            return resources;
        }

        internal /*for testing*/ bool GetMsiResourcesForPatchedFiles(IEnumerable<string> patchedFiles, HashSet<MsiResource> resources)
        {
            bool anyUnknowns = false;

            foreach (string patchedFile in patchedFiles)
            {
                var patchedResources = this.pdbManager.FindMsiResourcesForSourcePath(patchedFile);
                anyUnknowns |= AddCommand.AddResources(patchedFile, patchedResources, resources);
            }

            return anyUnknowns;
        }

        internal /*for testing*/ bool GetMsiResourcesForPatchedComponents(IEnumerable<string> patchedComponents, HashSet<MsiResource> resources)
        {
            bool anyUnknowns = false;

            foreach (string patchedComponent in patchedComponents)
            {
                var patchedResources = this.pdbManager.FindMsiResourcesForComponent(patchedComponent);
                anyUnknowns |= AddCommand.AddResources(patchedComponent, patchedResources, resources);
            }

            return anyUnknowns;
        }

        private static bool AddResources(string patchedResourceName, IEnumerable<MsiResource> patchedResources, HashSet<MsiResource> resources)
        {
            if (patchedResources.Any())
            {
                foreach (var resourceGroup in patchedResources.GroupBy(resource => resource.ComponentId))
                {
                    UX.Message("Patching component {0}", resourceGroup.Key);
                    foreach (var resource in resourceGroup)
                    {
                        resources.Add(resource);
                    }
                }

                return false;
            }
            else
            {
                UX.Warn("Unknown resource {0}", patchedResourceName);
                return true;
            }
        }

        /// <summary>
        /// If a newly patched component is in an existing patch family with components from other SKUs, those other SKUs need to be targeted too.
        /// </summary>
        /// <param name="patchFamilyFiles"></param>
        /// <param name="existingResources"></param>
        internal /*for testing*/ void AddMsiResourcesForPatchFamilies(IEnumerable<PatchFamilyFile> patchFamilyFiles, HashSet<MsiResource> existingResources)
        {
            foreach (PatchFamilyFile patchFamilyFile in patchFamilyFiles)
            {
                foreach (string componentId in patchFamilyFile.PatchFamilyChildren)
                {
                    foreach (MsiResource resource in this.pdbManager.FindMsiResourcesForComponent(componentId))
                    {
                        if (!WixProject.IsInvalidProject(resource.Sku.WixprojPath, this.projectManager.AllInclusivePatch))
                        {
                            existingResources.Add(resource);
                        }
                        else
                        {
                            // If WixProj is invalid, then don't add it to the MSIResources
                            UX.Trace("Resource {0} from SKU {1} is a candidate for adding since it is in the same patch family of a resource being added. It is not being added because the project isn't eligible for patching: {2}", resource.ComponentId, resource.Sku.SkuKey, resource.Sku.WixprojPath);
                        }
                    }
                }
            }
        }
    }

