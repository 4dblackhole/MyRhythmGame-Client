#include "EditorAnalysisController.h"
#include "MRG_Core.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
namespace chart = finger_drum::chart;

struct EditorAnalysisJob
{
    std::stop_source cancellation;
    std::atomic<bool> ready{};
    std::shared_ptr<const AnalysisBatch> result;
    std::exception_ptr failure;
};

namespace
{
    // One process-owned worker bounds concurrency. Jobs own only copied paths and
    // result state, so exiting an editor never joins or leaves dangling observers.
    class AudioAnalysisWorker final
    {
    public:
        static AudioAnalysisWorker &Instance()
        {
            static AudioAnalysisWorker worker;
            return worker;
        }
        void Enqueue(std::function<void()> operation)
        {
            {
                std::scoped_lock lock(mutex_);
                operations_.push_back(std::move(operation));
            }
            wake_.notify_one();
        }
    private:
        AudioAnalysisWorker() : thread_([this](std::stop_token stop) { Run(stop); }) {}
        ~AudioAnalysisWorker()
        {
            thread_.request_stop();
            wake_.notify_all();
        }
        void Run(std::stop_token stop)
        {
            while (!stop.stop_requested())
            {
                std::function<void()> operation;
                if (!WaitAndDequeue(stop, operation))
                    break;
                // Queue access has finished and its lock has left scope before
                // running work, including destruction of retired audio data.
                try { operation(); }
                catch (...) { /* Keep the worker available if a cleanup task fails. */ }
            }
        }
        bool WaitAndDequeue(std::stop_token stop, std::function<void()> &operation)
        {
            std::unique_lock lock(mutex_);
            if (!wake_.wait(lock, stop, [this] { return !operations_.empty(); }))
                return false;
            operation = std::move(operations_.front());
            operations_.pop_front();
            // Both the dequeue and cancellation paths release through the same
            // RAII lifetime. No manual unlock is mixed with scope-based release.
            return true;
        }
        std::mutex mutex_;
        std::condition_variable_any wake_;
        std::deque<std::function<void()>> operations_;
        std::jthread thread_;
    };

    void AnalyzeFiles(const std::shared_ptr<EditorAnalysisJob> &job,
                      const std::map<std::string, std::filesystem::path> &files)
    {
        const auto stop = job->cancellation.get_token();
        if (stop.stop_requested())
            return;
        auto result = std::make_shared<AnalysisBatch>();
        for (const auto &[id, path] : files)
        {
            if (stop.stop_requested())
                return;
            try
            {
                result->sounds.emplace(id, finger_drum::editor::AnalyzeAudio(path, stop, mrg::audio::DecodeAudioPcm));
            }
            catch (const std::exception &error)
            {
                result->errors += id + ": " + error.what() + "; ";
            }
        }
        job->result = std::move(result);
        job->ready.store(true, std::memory_order_release);
    }

    void RetireAnalysis(std::shared_ptr<const AnalysisBatch> result) noexcept
    {
        if (!result)
            return;
        try
        {
            AudioAnalysisWorker::Instance().Enqueue([result = std::move(result)] {});
        }
        catch (...)
        {
            // Resource release must remain safe during low-memory shutdown.
        }
    }
} // namespace

void EditorAnalysisController::Stop() noexcept
{
    if (job_)
    {
        job_->cancellation.request_stop();
        if (job_->ready.load(std::memory_order_acquire))
            RetireAnalysis(std::move(job_->result));
        job_.reset();
    }
    RetireAnalysis(std::move(analysis_));
    analysisFiles.clear();
    ++filesRevision_;
    analysisDirty = true;
    ++analysisRevision_;
}

bool EditorAnalysisController::Update(const chart::IEditorDocument &document, const IEditorAudioSource &mode,
                                      const finger_drum::GameplayLaunchRequest &request, std::string &status)
{
    if (analysisDirty)
    {
        analysisDirty = false;
        auto files = mode.AudioFiles(document, request);
        if (files != analysisFiles)
        {
            if (job_)
            {
                job_->cancellation.request_stop();
                if (job_->ready.load(std::memory_order_acquire))
                    RetireAnalysis(std::move(job_->result));
            }
            auto next = std::make_shared<EditorAnalysisJob>();
            AudioAnalysisWorker::Instance().Enqueue([next, files] {
                try { AnalyzeFiles(next, files); }
                catch (...)
                {
                    next->failure = std::current_exception();
                    next->ready.store(true, std::memory_order_release);
                }
            });
            job_ = std::move(next);
            analysisFiles = std::move(files);
            ++filesRevision_;
        }
    }
    if (!job_ || !job_->ready.load(std::memory_order_acquire))
        return false;
    if (job_->failure)
    {
        try { std::rethrow_exception(job_->failure); }
        catch (const std::exception &error) { status = error.what(); }
        catch (...) { status = "Audio analysis failed."; }
        job_.reset();
        return true;
    }
    RetireAnalysis(std::move(analysis_));
    analysis_ = std::move(job_->result);
    analyzedFilesRevision_ = filesRevision_;
    ++analysisRevision_;
    job_.reset();
    if (!analysis_->errors.empty())
        status = analysis_->errors;
    return true;
}

std::optional<double> EditorAnalysisController::MusicDurationSeconds() const
{
    // Old FFT data may stay visible while a new source is analyzed, but it
    // must never supply a seek destination for the replacement music file.
    if (analysisDirty || job_ || !analysis_ || analyzedFilesRevision_ != filesRevision_)
        return std::nullopt;
    const auto music = analysis_->sounds.find("Music");
    return music == analysis_->sounds.end() ? std::nullopt : std::optional{music->second.durationSeconds};
}

void EditorAnalysisController::CacheAudioMarkers(const chart::IEditorDocument &document, const IEditorAudioSource &mode)
{
    if (audioMarkerDocument == &document && audioMarkerMode == &mode && audioMarkerRevision == document.Revision())
        return;
    auto markers = mode.AudioMarkers(document);
    audioMarkers = std::move(markers);
    audioMarkerDocument = &document;
    audioMarkerMode = &mode;
    audioMarkerRevision = document.Revision();
}
