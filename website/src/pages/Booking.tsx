import { useRef, useState, type ChangeEvent, type FormEvent, type ReactNode } from "react";
import { booking, contact } from "../config/site";
import { PageHeader } from "../components/ui";
import {
  MAX,
  emptyBooking,
  eventTypes,
  submitBooking,
  validateBooking,
  type BookingErrors,
  type BookingField,
  type BookingValues,
} from "../lib/booking";
import { todayISO } from "../lib/format";
import { usePageMeta } from "../lib/meta";
import { Link } from "../lib/router";

type Phase = "idle" | "sending" | "sent" | "failed";

const labels: Record<BookingField, string> = {
  name: "Contact person",
  email: "Email address",
  organization: "Promoter / organization",
  eventName: "Event name",
  eventDate: "Event date",
  location: "Venue and location",
  eventType: "Event type",
  budget: "Estimated budget",
  message: "Message",
  consent: "Privacy consent",
};

function Field({
  id,
  label,
  required,
  error,
  hint,
  children,
}: {
  id: string;
  label: string;
  required?: boolean;
  error?: string;
  hint?: string;
  children: ReactNode;
}) {
  return (
    <div className={`field${error ? " has-error" : ""}`}>
      <label htmlFor={id}>
        {label} {required ? <span className="req" aria-hidden="true">*</span> : <span className="opt">(optional)</span>}
      </label>
      {children}
      {hint && !error && (
        <p className="field__hint" id={`${id}-hint`}>
          {hint}
        </p>
      )}
      {error && (
        <p className="field__error" id={`${id}-error`}>
          {error}
        </p>
      )}
    </div>
  );
}

export function Booking() {
  usePageMeta({ title: "Booking", description: "Send a booking request to BrainBugz for your club night, festival or event." });
  const [values, setValues] = useState<BookingValues>(emptyBooking);
  const [errors, setErrors] = useState<BookingErrors>({});
  const [phase, setPhase] = useState<Phase>("idle");
  const [bot, setBot] = useState("");
  const summaryRef = useRef<HTMLDivElement>(null);
  const resultRef = useRef<HTMLDivElement>(null);
  const today = todayISO();
  const enabled = booking.provider === "netlify";

  const set = (field: BookingField) => (e: ChangeEvent<HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement>) => {
    const v = e.target instanceof HTMLInputElement && e.target.type === "checkbox" ? e.target.checked : e.target.value;
    setValues((prev) => ({ ...prev, [field]: v }));
    if (errors[field]) setErrors((prev) => ({ ...prev, [field]: undefined }));
  };

  const describedBy = (f: BookingField, hint = false) =>
    errors[f] ? `bk-${f}-error` : hint ? `bk-${f}-hint` : undefined;

  async function onSubmit(e: FormEvent) {
    e.preventDefault();
    if (phase === "sending") return;
    const errs = validateBooking(values, today);
    setErrors(errs);
    if (Object.keys(errs).length) {
      setPhase("idle");
      requestAnimationFrame(() => summaryRef.current?.focus());
      return;
    }
    setPhase("sending");
    const res = await submitBooking(booking.formName, values, bot);
    setPhase(res.ok ? "sent" : "failed");
    requestAnimationFrame(() => resultRef.current?.focus());
  }

  const errorList = (Object.keys(errors) as BookingField[]).filter((k) => errors[k]);

  return (
    <div className="container page">
      <PageHeader
        code="// BOOKING"
        title="Booking"
        intro="Clubs, festivals, open airs and private events: tell us about your event and we'll get back to you by email."
      />

      {!enabled ? (
        <div className="notice" role="status">
          <p>Online booking requests aren't open yet.</p>
          {contact.publicBookingEmail && (
            <p>
              Please email us at <a href={`mailto:${contact.publicBookingEmail}`}>{contact.publicBookingEmail}</a>.
            </p>
          )}
        </div>
      ) : phase === "sent" ? (
        <div className="notice notice--success" role="status" tabIndex={-1} ref={resultRef}>
          <h2>Request sent</h2>
          <p>
            Thanks, {values.name.split(" ")[0]}! Your booking request for {values.eventDate} has been received. We'll reply to{" "}
            <strong>{values.email}</strong>.
          </p>
          <button
            type="button"
            className="btn btn--ghost btn--sm"
            onClick={() => {
              setValues(emptyBooking);
              setPhase("idle");
            }}
          >
            Send another request
          </button>
        </div>
      ) : (
        <form
          className="booking-form"
          name={booking.formName}
          method="POST"
          data-netlify="true"
          netlify-honeypot="bot-field"
          noValidate
          onSubmit={onSubmit}
          aria-describedby="bk-required-note"
        >
          <input type="hidden" name="form-name" value={booking.formName} />
          <p className="hp" aria-hidden="true">
            <label>
              Leave this empty: <input name="bot-field" tabIndex={-1} autoComplete="off" value={bot} onChange={(e) => setBot(e.target.value)} />
            </label>
          </p>

          {errorList.length > 0 && (
            <div className="notice notice--error" role="alert" tabIndex={-1} ref={summaryRef}>
              <p>
                <strong>Please check {errorList.length === 1 ? "this field" : `these ${errorList.length} fields`}:</strong>
              </p>
              <ul>
                {errorList.map((k) => (
                  <li key={k}>
                    <a href={`#bk-${k}`}>{labels[k]}</a>: {errors[k]}
                  </li>
                ))}
              </ul>
            </div>
          )}

          {phase === "failed" && (
            <div className="notice notice--error" role="alert" tabIndex={-1} ref={resultRef}>
              <p>
                <strong>Your request could not be sent.</strong> Nothing was submitted. Please try again in a moment.
              </p>
              {contact.publicBookingEmail && (
                <p>
                  You can also email us at <a href={`mailto:${contact.publicBookingEmail}`}>{contact.publicBookingEmail}</a>.
                </p>
              )}
            </div>
          )}

          <p id="bk-required-note" className="muted small">
            Fields marked <span className="req">*</span> are required.
          </p>

          <fieldset>
            <legend>You</legend>
            <div className="form-grid">
              <Field id="bk-name" label={labels.name} required error={errors.name}>
                <input id="bk-name" name="name" autoComplete="name" maxLength={MAX.short} value={values.name} onChange={set("name")} aria-invalid={!!errors.name} aria-describedby={describedBy("name")} required />
              </Field>
              <Field id="bk-email" label={labels.email} required error={errors.email}>
                <input id="bk-email" name="email" type="email" autoComplete="email" inputMode="email" maxLength={254} value={values.email} onChange={set("email")} aria-invalid={!!errors.email} aria-describedby={describedBy("email")} required />
              </Field>
              <Field id="bk-organization" label={labels.organization} error={errors.organization}>
                <input id="bk-organization" name="organization" autoComplete="organization" maxLength={MAX.short} value={values.organization} onChange={set("organization")} aria-invalid={!!errors.organization} aria-describedby={describedBy("organization")} />
              </Field>
            </div>
          </fieldset>

          <fieldset>
            <legend>Your event</legend>
            <div className="form-grid">
              <Field id="bk-eventName" label={labels.eventName} error={errors.eventName}>
                <input id="bk-eventName" name="eventName" maxLength={MAX.short} value={values.eventName} onChange={set("eventName")} aria-invalid={!!errors.eventName} aria-describedby={describedBy("eventName")} />
              </Field>
              <Field id="bk-eventDate" label={labels.eventDate} required error={errors.eventDate}>
                <input id="bk-eventDate" name="eventDate" type="date" min={today} value={values.eventDate} onChange={set("eventDate")} aria-invalid={!!errors.eventDate} aria-describedby={describedBy("eventDate")} required />
              </Field>
              <Field id="bk-location" label={labels.location} required error={errors.location} hint="Venue, city and country">
                <input id="bk-location" name="location" maxLength={MAX.short} value={values.location} onChange={set("location")} aria-invalid={!!errors.location} aria-describedby={describedBy("location", true)} required />
              </Field>
              <Field id="bk-eventType" label={labels.eventType} required error={errors.eventType}>
                <select id="bk-eventType" name="eventType" value={values.eventType} onChange={set("eventType")} aria-invalid={!!errors.eventType} aria-describedby={describedBy("eventType")} required>
                  <option value="">Please choose…</option>
                  {eventTypes.map((t) => (
                    <option key={t} value={t}>
                      {t}
                    </option>
                  ))}
                </select>
              </Field>
              <Field id="bk-budget" label={labels.budget} error={errors.budget} hint="e.g. fee range incl. travel">
                <input id="bk-budget" name="budget" maxLength={MAX.short} value={values.budget} onChange={set("budget")} aria-invalid={!!errors.budget} aria-describedby={describedBy("budget", true)} />
              </Field>
            </div>
            <Field id="bk-message" label={labels.message} required error={errors.message} hint="Line-up, set length, technical setup, anything else we should know">
              <textarea id="bk-message" name="message" rows={6} maxLength={MAX.message} value={values.message} onChange={set("message")} aria-invalid={!!errors.message} aria-describedby={describedBy("message", true)} required />
            </Field>
          </fieldset>

          <div className={`field${errors.consent ? " has-error" : ""}`}>
            <label className="check" htmlFor="bk-consent">
              <input id="bk-consent" name="consent" type="checkbox" checked={values.consent} onChange={set("consent")} aria-invalid={!!errors.consent} aria-describedby={describedBy("consent")} required />
              <span>
                I agree that my details are used to process this booking request, as described in the{" "}
                <Link to="/datenschutz">privacy policy</Link>. <span className="req" aria-hidden="true">*</span>
              </span>
            </label>
            {errors.consent && (
              <p className="field__error" id="bk-consent-error">
                {errors.consent}
              </p>
            )}
          </div>

          <div className="btn-row">
            <button type="submit" className="btn btn--primary btn--lg" disabled={phase === "sending"} aria-busy={phase === "sending"}>
              {phase === "sending" ? "Sending…" : "Send booking request"}
            </button>
          </div>
        </form>
      )}
    </div>
  );
}
