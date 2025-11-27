#include <iostream>

#include "Core/Window.h"
#include "Debug/Log.h"

#include "Render/RHI/RHIRenderer.h"
#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/ResourceManager.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"

#include "Utils/Type.h"

int Run(int argc, char** argv, char** envp)
{
    (void)argc;
    (void)argv;
    (void)envp;

    WindowConfig config;
    config.title = "Window Test";
    config.size = Vec2i(1280, 720);
    config.attributes = static_cast<WindowAttributes>(VSync);
    std::unique_ptr<Window> window = Window::Create(WindowAPI::GLFW, RenderAPI::Vulkan, config);

    if (!window)
    {
        std::cerr << "Failed to create window" << '\n';
        return -1;
    }

    std::unique_ptr<RHIRenderer> renderer = RHIRenderer::Create(RenderAPI::Vulkan, window.get());
    if (!renderer)
    {
        std::cerr << "Failed to create renderer" << '\n';
        return -1;
    }

    ThreadPool::Initialize();

    std::unique_ptr<ResourceManager> resourceManager = std::make_unique<ResourceManager>();
    resourceManager->Initialize(renderer.get());
    resourceManager->LoadDefaultTexture("resources/textures/debug.jpeg");
    
    renderer->SetDefaultTexture(resourceManager->GetDefaultTexture());

    SafePtr<Model> cubeModel = resourceManager->Load<Model>("resources/models/Cube.obj");
    SafePtr cubeTexture = resourceManager->GetDefaultTexture();
    // SafePtr cubeShader = resourceManager->GetDefaultShader();

    dynamic_cast<VulkanRenderer*>(renderer.get())->SetModel(cubeModel);
    dynamic_cast<VulkanRenderer*>(renderer.get())->SetTexture(cubeTexture);

    while (!window->ShouldClose())
    {
        window->PollEvents();

        resourceManager->UpdateResourceToSend();

        if (!renderer->IsInitialized())
            continue;
        
        renderer->WaitUntilFrameFinished();
        renderer->Update();
        if (!renderer->BeginFrame())
            continue;
        
        renderer->DrawFrame();
        
        renderer->EndFrame();
    }

    // Wait for GPU to finish rendering before cleaning
    renderer->WaitForGPU();
    resourceManager->Clear();
    renderer->Cleanup();

    ThreadPool::Terminate();

    window->Terminate();

    return 0;
}


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(_WIN32) && defined(NDEBUG)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
#else
int main(int argc, char** argv, char** envp)
#endif
{
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // TODO: Remove Comments To Break on leaks
    // |
    // V
    // _CrtSetBreakAlloc(173);
#ifdef NDEBUG
    int argc = __argc;
    char** argv = __argv;
    char** envp = nullptr;
#endif
#endif
    return Run(argc, argv, envp);
}

/*
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>

using namespace std;

enum Exercice { FESSIER, PECS, BRAS, DOS, JAMBES };

string exerciceToString(Exercice e) {
    switch(e) {
        case FESSIER: return "Fessier";
        case PECS: return "Pecs";
        case BRAS: return "Bras";
        case DOS: return "Dos";
        case JAMBES: return "Jambes";
    }
    return "";
}

class Personne {
public:
    string nom;
    map<Exercice, int> exercicesRestants;
    Exercice dernierExo;
    bool aDernierExo;
    
    Personne(string n) : nom(n), aDernierExo(false) {}
    
    bool peutFaire(Exercice e) {
        if (exercicesRestants[e] <= 0) return false;
        if (!aDernierExo) return true;
        
        // Fessier et Jambes ne peuvent pas s'enchaîner
        if ((dernierExo == FESSIER && e == JAMBES) || 
            (dernierExo == JAMBES && e == FESSIER)) {
            return false;
        }
        
        // Un exercice ne peut pas se répéter 2 jours de suite
        if (dernierExo == e) return false;
        
        return true;
    }
    
    void faireExercice(Exercice e) {
        exercicesRestants[e]--;
        dernierExo = e;
        aDernierExo = true;
    }
    
    void resetJour() {
        aDernierExo = false;
    }
};

bool genererPlanning(vector<string>& jours, int jourRepos, 
                     vector<pair<Exercice, Exercice>>& planning,
                     Personne& p1, Personne& p2, int jourActuel) {
    
    // Cas de base : planning complet
    if (jourActuel >= 7) {
        return true;
    }
    
    // Jour de repos
    if (jourActuel == jourRepos) {
        planning.push_back({(Exercice)-1, (Exercice)-1});
        return genererPlanning(jours, jourRepos, planning, p1, p2, jourActuel + 1);
    }
    
    // Samedi = Jambes obligatoire
    if (jourActuel == 5) { // Samedi
        if (p1.peutFaire(JAMBES) && p2.peutFaire(JAMBES)) {
            p1.faireExercice(JAMBES);
            p2.faireExercice(JAMBES);
            planning.push_back({JAMBES, JAMBES});
            
            bool resultat = genererPlanning(jours, jourRepos, planning, p1, p2, jourActuel + 1);
            if (resultat) return true;
            
            // Backtrack
            p1.exercicesRestants[JAMBES]++;
            p2.exercicesRestants[JAMBES]++;
            planning.pop_back();
        }
        return false;
    }
    
    // Essayer tous les exercices possibles
    vector<Exercice> exos = {FESSIER, PECS, BRAS, DOS, JAMBES};
    
    for (Exercice e1 : exos) {
        if (!p1.peutFaire(e1)) continue;
        
        for (Exercice e2 : exos) {
            if (!p2.peutFaire(e2)) continue;
            
            // Priorité : même exercice
            bool memeExo = (e1 == e2);
            
            p1.faireExercice(e1);
            p2.faireExercice(e2);
            planning.push_back({e1, e2});
            
            bool resultat = genererPlanning(jours, jourRepos, planning, p1, p2, jourActuel + 1);
            if (resultat) return true;
            
            // Backtrack
            p1.exercicesRestants[e1]++;
            p2.exercicesRestants[e2]++;
            p1.dernierExo = (jourActuel > 0 && planning.size() > 1) ? planning[jourActuel-1].first : (Exercice)-1;
            p2.dernierExo = (jourActuel > 0 && planning.size() > 1) ? planning[jourActuel-1].second : (Exercice)-1;
            p1.aDernierExo = (jourActuel > 0);
            p2.aDernierExo = (jourActuel > 0);
            planning.pop_back();
        }
    }
    
    return false;
}

int main() {
    vector<string> jours = {"Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"};
    
    // Initialisation Personne 1
    Personne p1("Personne 1");
    p1.exercicesRestants[FESSIER] = 2;
    p1.exercicesRestants[PECS] = 1;
    p1.exercicesRestants[BRAS] = 1;
    p1.exercicesRestants[DOS] = 1;
    p1.exercicesRestants[JAMBES] = 1;
    
    // Initialisation Personne 2
    Personne p2("Personne 2");
    p2.exercicesRestants[PECS] = 2;
    p2.exercicesRestants[BRAS] = 2; // On essaiera 1 ou 2
    p2.exercicesRestants[DOS] = 1;  // On essaiera 1 ou 2
    p2.exercicesRestants[JAMBES] = 1;
    p2.exercicesRestants[FESSIER] = 0;
    
    cout << "=== PLANNING D'ENTRAINEMENT HEBDOMADAIRE ===" << endl << endl;
    
    // Essayer avec différentes configurations
    bool trouve = false;
    
    // Configuration 1 : Bras=2, Dos=1, Repos=Mercredi
    for (int jourRepos = 2; jourRepos <= 3 && !trouve; jourRepos++) {
        for (int bras = 1; bras <= 2 && !trouve; bras++) {
            int dos = 3 - bras; // Si bras=1 alors dos=2, si bras=2 alors dos=1
            
            Personne p1_copy = p1;
            Personne p2_copy = p2;
            p2_copy.exercicesRestants[BRAS] = bras;
            p2_copy.exercicesRestants[DOS] = dos;
            
            vector<pair<Exercice, Exercice>> planning;
            
            if (genererPlanning(jours, jourRepos, planning, p1_copy, p2_copy, 0)) {
                trouve = true;
                
                cout << "Jour de repos : " << jours[jourRepos] << endl;
                cout << "Personne 2 : " << bras << "x Bras, " << dos << "x Dos" << endl << endl;
                
                for (int i = 0; i < 7; i++) {
                    cout << jours[i] << " : ";
                    if (i == jourRepos) {
                        cout << "REPOS" << endl;
                    } else {
                        cout << "P1=" << exerciceToString(planning[i].first) 
                             << " | P2=" << exerciceToString(planning[i].second);
                        if (planning[i].first == planning[i].second) {
                            cout << " ✓ (Ensemble)";
                        }
                        cout << endl;
                    }
                }
                
                // Vérification
                cout << "\n--- Verification des totaux ---" << endl;
                map<Exercice, int> total1, total2;
                for (auto& p : planning) {
                    if (p.first != (Exercice)-1) total1[p.first]++;
                    if (p.second != (Exercice)-1) total2[p.second]++;
                }
                
                cout << "Personne 1: ";
                for (auto& e : total1) {
                    cout << exerciceToString(e.first) << "=" << e.second << " ";
                }
                cout << "\nPersonne 2: ";
                for (auto& e : total2) {
                    cout << exerciceToString(e.first) << "=" << e.second << " ";
                }
                cout << endl;
            }
        }
    }
    
    if (!trouve) {
        cout << "Impossible de générer un planning valide avec ces contraintes." << endl;
    }
    
    return 0;
}
*/
